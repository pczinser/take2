// sim_entry.cpp
// Entry TU with extension registration and scheduler

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/message.h>

#include "core/sim_time.hpp"
#include "activation/activation.hpp"
#include "world/world.hpp"
#include "world/entity.hpp"
#include "observer/observer.hpp"
#include "systems/portal_system.hpp"
#include "core/events.hpp"
#include "systems/extractor_system.hpp"
#include "systems/inventory_system.hpp"
#include "items.hpp"
#include "components/component_registry.hpp"

#include "lua/lua_bindings.hpp"   // for all Lua registrations
#include "sim_entry.hpp"          // header declaring the C API used by lua_sim.cpp
#include "command_queue.hpp"      // for command queue integration

namespace simcore {

// ─────────────────────────────────────────────────────────────────────────────
// SIM STATE
// ─────────────────────────────────────────────────────────────────────────────
static double         g_hz                   = 60.0;
static float          s_fixed_dt             = 1.0f / 60.0f;
static uint32_t       s_current_tick         = 0;
static float          s_last_alpha           = 0.0f;
static int            s_debug_ticks_printed  = 0;

static bool           s_paused               = false;

static dmMessage::URL s_listener_url         = {};
static bool           s_listener_registered  = false;

// Two snapshot buffers: prev (last tick) and curr (this tick)
static dmBuffer::HBuffer s_prev_snapshot     = 0;
static dmBuffer::HBuffer s_curr_snapshot     = 0;
static uint32_t          s_curr_rows         = 0;

// Message id
static const dmhash_t SIM_TICK = dmHashString64("sim_tick");

// ─────────────────────────────────────────────────────────────────────────────
// SNAPSHOT WRITER
// ─────────────────────────────────────────────────────────────────────────────

static void CreateAndFillSnapshot(dmBuffer::HBuffer& out_buf) {
    // Build a brand-new buffer for the current tick, with the exact row count
    const auto& entities = GetAllEntities();
    const uint32_t rows = (uint32_t)entities.size();
    s_curr_rows = rows;

    if (rows == 0) {
        // No entities this tick
        out_buf = 0;
        return;
    }

    dmBuffer::StreamDeclaration decl[] = {
        { dmHashString64("id"),    dmBuffer::VALUE_TYPE_UINT32, 1 },
        { dmHashString64("x"),     dmBuffer::VALUE_TYPE_FLOAT32,1 },
        { dmHashString64("y"),     dmBuffer::VALUE_TYPE_FLOAT32,1 },
        { dmHashString64("z"),     dmBuffer::VALUE_TYPE_FLOAT32,1 },
        { dmHashString64("vx"),    dmBuffer::VALUE_TYPE_FLOAT32,1 },
        { dmHashString64("vy"),    dmBuffer::VALUE_TYPE_FLOAT32,1 },
        { dmHashString64("ang"),   dmBuffer::VALUE_TYPE_FLOAT32,1 }, // radians
        { dmHashString64("flags"), dmBuffer::VALUE_TYPE_UINT32, 1 },
    };

    dmBuffer::HBuffer buf = 0;
    dmBuffer::Result cr = dmBuffer::Create(rows, decl, (uint8_t)DM_ARRAY_SIZE(decl), &buf);
    if (cr != dmBuffer::RESULT_OK) {
        dmLogError("Failed to create snapshot buffer (rows=%u)", rows);
        out_buf = 0;
        return;
    }

    // Streams
    uint32_t *idp, *flagsp;
    float *xp, *yp, *zp, *vxp, *vyp, *angp;
    uint32_t count = 0, comps = 0, stride = 0;

    dmBuffer::GetStream(buf, dmHashString64("id"),    (void**)&idp,    &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("x"),     (void**)&xp,     &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("y"),     (void**)&yp,     &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("z"),     (void**)&zp,     &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("vx"),    (void**)&vxp,    &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("vy"),    (void**)&vyp,    &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("ang"),   (void**)&angp,   &count, &comps, &stride);
    dmBuffer::GetStream(buf, dmHashString64("flags"), (void**)&flagsp, &count, &comps, &stride);

    // Zero-initialize all streams to avoid uninitialized subnormal values
    memset(idp,    0, sizeof(uint32_t) * rows);
    memset(flagsp, 0, sizeof(uint32_t) * rows);
    memset(xp,     0, sizeof(float)    * rows);
    memset(yp,     0, sizeof(float)    * rows);
    memset(zp,     0, sizeof(float)    * rows);
    memset(vxp,    0, sizeof(float)    * rows);
    memset(vyp,    0, sizeof(float)    * rows);
    memset(angp,   0, sizeof(float)    * rows);

    // Fill rows
    for (uint32_t i = 0; i < rows; ++i) {
        const auto& row = entities[i];
        const Entity* ent = GetEntity(row.id);
        idp[i] = row.id;

        if (!ent) {
            xp[i]=yp[i]=zp[i]=vxp[i]=vyp[i]=angp[i]=0.0f;
            flagsp[i]=0u;
            continue;
        }

        // Read components
        const components::TransformComponent* t = components::g_transform_components.GetComponent(row.id);
        const components::AnimStateComponent* s = components::g_animstate_components.GetComponent(row.id);

        if (t) {
            xp[i]   = t->grid_x * 64.0f;  // Convert grid to world units
            yp[i]   = t->grid_y * 64.0f;
            zp[i]   = (float)t->floor_z;
            vxp[i]  = 0.0f;  // TODO: velocity from physics
            vyp[i]  = 0.0f;
        } else {
            xp[i]=yp[i]=zp[i]=vxp[i]=vyp[i]=0.0f;
        }
        angp[i] = s ? s->facing_angle : 0.0f;
        flagsp[i] = s ? s->flags : 0u;
    }

    out_buf = buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// SIM STEP + SIGNAL
// ─────────────────────────────────────────────────────────────────────────────

static inline void PostSimTick() {
    if (!s_listener_registered) return;
    // sender, receiver, message_id, user_data1, user_data2, descriptor, data, size, destroy_cb
    dmMessage::Post(0, &s_listener_url, SIM_TICK, 0, 0, 0, 0, 0, 0);
}


static void StepOneTick(float dt_fixed) {
    s_fixed_dt = dt_fixed;
    ++s_current_tick;

    // 1) Process any queued commands first (deterministic ordering)
    ProcessCommandQueue(s_current_tick);

    // 2) advance systems (authoritative)
    RebuildActivationUnion();
    Portal_Step((int32_t)(dt_fixed * 1000.0f), (int64_t)(NowSeconds() * 1000.0));
    Extractor_Step(dt_fixed);
    // Inventory_Tick(dt_fixed); // uncomment if you tick inventory here

    // 3) rotate buffers: destroy the older 'prev', move 'curr' to 'prev', create a new 'curr'
    if (s_prev_snapshot) {
        dmBuffer::Destroy(s_prev_snapshot);
        s_prev_snapshot = 0;
    }
    s_prev_snapshot = s_curr_snapshot; // last frame becomes 'prev'
    s_curr_snapshot = 0;
    CreateAndFillSnapshot(s_curr_snapshot);

    // Boot diagnostics: print first ticks' snapshot summary
    if (s_debug_ticks_printed < 8) {
        uint32_t *idp = 0; float *xp = 0, *yp = 0, *zp = 0; uint32_t n = 0, comps = 0, stride = 0;
        if (s_curr_snapshot) {
            dmBuffer::GetStream(s_curr_snapshot, dmHashString64("id"), (void**)&idp, &n, &comps, &stride);
            dmBuffer::GetStream(s_curr_snapshot, dmHashString64("x"),  (void**)&xp,  &n, &comps, &stride);
            dmBuffer::GetStream(s_curr_snapshot, dmHashString64("y"),  (void**)&yp,  &n, &comps, &stride);
            dmBuffer::GetStream(s_curr_snapshot, dmHashString64("z"),  (void**)&zp,  &n, &comps, &stride);
        }
        if (idp && n > 0) {
            dmLogInfo("SNAPSHOT tick=%u rows=%u first_id=%u pos=(%f,%f,%f)", s_current_tick, n, idp[0], xp[0], yp[0], zp[0]);
        } else {
            dmLogInfo("SNAPSHOT tick=%u rows=0", s_current_tick);
        }
        ++s_debug_ticks_printed;
    }

    // 4) clear edge-triggered event queues
    Events_Clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// DEFOLD EXTENSION LIFECYCLE
// ─────────────────────────────────────────────────────────────────────────────

static dmExtension::Result AppInitialize(dmExtension::AppParams*) { return dmExtension::RESULT_OK; }
static dmExtension::Result AppFinalize  (dmExtension::AppParams*) { return dmExtension::RESULT_OK; }

static dmExtension::Result Initialize(dmExtension::Params* params) {
    TimeInit();

    // world & systems
    InitializeEntitySystem();
    Portal_Init();
    Extractor_Init();
    Items_Init();
    Inventory_Init();

    // Register Lua API
    LuaRegister_All(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params*) {
    if (s_prev_snapshot) dmBuffer::Destroy(s_prev_snapshot);
    if (s_curr_snapshot) dmBuffer::Destroy(s_curr_snapshot);
    s_prev_snapshot = s_curr_snapshot = 0;
    s_curr_rows = 0;
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Update(dmExtension::Params*) {
    if (s_paused) return dmExtension::RESULT_OK;

    int steps = 0;
    double alpha = FixedStep([&](float dt) {
        StepOneTick(dt);
        ++steps;
    }, 1.0 / g_hz);

    s_last_alpha = (float)alpha;
    if (steps > 0) {
        PostSimTick();
    }
    return dmExtension::RESULT_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// C API FOR LUA BINDINGS (called by lua_sim.cpp)
// ─────────────────────────────────────────────────────────────────────────────

void SetListenerURL(const dmMessage::URL& url) {
    s_listener_url = url;
    s_listener_registered = true;
}

// Legacy single-buffer getter (current)
dmBuffer::HBuffer GetSnapshotBuffer()              { return s_curr_snapshot; }

// Recommended: both buffers for interpolation
dmBuffer::HBuffer GetSnapshotBufferCurrent()       { return s_curr_snapshot; }
dmBuffer::HBuffer GetSnapshotBufferPrevious()      { return s_prev_snapshot; }
uint32_t          GetSnapshotRowCount()            { return s_curr_rows; }
uint32_t          GetCurrentTick()                 { return s_current_tick; }
float             GetFixedDt()                     { return s_fixed_dt; }
float             GetLastAlpha()                   { return s_last_alpha; }

// Optional controls (debug)
void SetSimHz(double hz) {
    if (hz < 1.0)   hz = 1.0;
    if (hz > 480.0) hz = 480.0;
    g_hz = hz;
}
void SetSimPaused(bool paused) { s_paused = paused; }
void StepSimNTicks(int n) {
    if (!s_paused) return;
    if (n < 0) n = 0;
    for (int i = 0; i < n; ++i) StepOneTick(s_fixed_dt);
    // Let Lua consume the manual progression
    PostSimTick();
}

} // namespace simcore

DM_DECLARE_EXTENSION(sim, "sim",
    simcore::AppInitialize, simcore::AppFinalize,
    simcore::Initialize,    simcore::Update,
    0,
    simcore::Finalize)
