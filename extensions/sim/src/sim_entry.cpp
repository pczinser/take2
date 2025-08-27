// sim_entry.cpp
// Entry TU with extension registration and scheduler

// ═══════════════════════════════════════════════════════════════════════════════
// SNAPSHOT BUFFER SYSTEM DOCUMENTATION
// ═══════════════════════════════════════════════════════════════════════════════
//
// This file implements a single contiguous data stream approach for entity snapshots
// to avoid Defold's multi-stream interleaving issues that caused memory corruption.
//
// HOW TO ADD NEW DATA FIELDS:
// ═══════════════════════════════════════════════════════════════════════════════
//
// 1. Update ENTITY_FIELD_COUNT and add new FIELD_* constants:
//    #define ENTITY_FIELD_COUNT 9    // Increment from 8 to 9
//    #define FIELD_NEW_DATA 8        // Add after FIELD_FLAGS
//
// 2. Update buffer initialization in CreateAndFillSnapshot():
//    datap[base + FIELD_NEW_DATA] = 0.0f;
//
// 3. Update entity filling loop:
//    datap[base + FIELD_NEW_DATA] = get_new_data_value(ent);
//
// 4. Update Lua extraction in visual_manager.script:
//    streams.curr.new_data[i] = data_stream[base + 9]  -- 9th field
//
// 5. Update Lua processing in visual_manager.lua:
//    local new_data = data_stream[base + 9]
//
// MEMORY LAYOUT:
// ═══════════════════════════════════════════════════════════════════════════════
// Each entity occupies ENTITY_FIELD_COUNT consecutive floats in the data stream:
// [id, x, y, z, vx, vy, ang, flags, new_data, ...]
//
// Access pattern: datap[entity_index * ENTITY_FIELD_COUNT + field_offset]
// ═══════════════════════════════════════════════════════════════════════════════

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/message.h>
#include <cmath>

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
    
    // Validate entity IDs (log only invalid ones)
    for (uint32_t i = 0; i < rows; ++i) {
        if (entities[i].id == 0) {
            dmLogWarning("Entity[%u] has invalid ID=0", i);
        }
    }

    if (rows == 0) {
        // No entities this tick
        out_buf = 0;
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════════════
    // BUFFER ARCHITECTURE: Single Contiguous Data Stream
    // ═══════════════════════════════════════════════════════════════════════════════
    // 
    // We use a single "data" stream with 8 float32 components per entity to avoid
    // Defold's multi-stream interleaving issues that caused memory corruption.
    // 
    // Layout per entity (8 consecutive floats):
    //   [0] id    - Entity ID (stored as float, convert to uint32 in Lua)
    //   [1] x     - World X coordinate  
    //   [2] y     - World Y coordinate
    //   [3] z     - World Z coordinate (floor level)
    //   [4] vx    - X velocity component
    //   [5] vy    - Y velocity component  
    //   [6] ang   - Rotation angle (radians)
    //   [7] flags - Entity flags (stored as float, convert to uint32 in Lua)
    //
    // Memory layout: [ent0_id, ent0_x, ent0_y, ent0_z, ent0_vx, ent0_vy, ent0_ang, ent0_flags,
    //                 ent1_id, ent1_x, ent1_y, ent1_z, ent1_vx, ent1_vy, ent1_ang, ent1_flags, ...]
    //
    // Access pattern: datap[entity_index * 8 + field_offset]
    // ═══════════════════════════════════════════════════════════════════════════════

    #define ENTITY_FIELD_COUNT 8
    #define FIELD_ID    0
    #define FIELD_X     1  
    #define FIELD_Y     2
    #define FIELD_Z     3
    #define FIELD_VX    4
    #define FIELD_VY    5
    #define FIELD_ANG   6
    #define FIELD_FLAGS 7

    dmBuffer::StreamDeclaration decl[] = {
        { dmHashString64("data"), dmBuffer::VALUE_TYPE_FLOAT32, ENTITY_FIELD_COUNT },
    };

    dmBuffer::HBuffer buf = 0;
    dmBuffer::Result cr = dmBuffer::Create(rows, decl, 1, &buf);
    if (cr != dmBuffer::RESULT_OK) {
        dmLogError("Failed to create snapshot buffer (rows=%u)", rows);
        out_buf = 0;
        return;
    }
    
    if (buf == 0) {
        dmLogError("Buffer creation returned null handle");
        out_buf = 0;
        return;
    }

    // Get the data stream
    float *datap;
    uint32_t count = 0, comps = 0, stride = 0;
    dmBuffer::Result sr = dmBuffer::GetStream(buf, dmHashString64("data"), (void**)&datap, &count, &comps, &stride);
    
    if (!datap || count != rows) {
        dmLogError("Failed to get valid stream pointer or count mismatch (expected %u, got %u)", rows, count);
        dmBuffer::Destroy(buf);
        out_buf = 0;
        return;
    }

    // Zero-initialize all data to avoid uninitialized values
    for (uint32_t i = 0; i < rows; ++i) {
        uint32_t base = i * ENTITY_FIELD_COUNT;
        datap[base + FIELD_ID]    = 0.0f;
        datap[base + FIELD_X]     = 0.0f;
        datap[base + FIELD_Y]     = 0.0f;
        datap[base + FIELD_Z]     = 0.0f;
        datap[base + FIELD_VX]    = 0.0f;
        datap[base + FIELD_VY]    = 0.0f;
        datap[base + FIELD_ANG]   = 0.0f;
        datap[base + FIELD_FLAGS] = 0.0f;
    }

    // Fill entity data
    for (uint32_t i = 0; i < rows && i < count; ++i) {
        const auto& row = entities[i];
        const Entity* ent = GetEntity(row.id);
        uint32_t base = i * ENTITY_FIELD_COUNT;

        if (!ent || row.id == 0) {
            // Entity not found or invalid ID - leave as zeros (already initialized)
            continue;
        }

        // Set entity ID
        datap[base + FIELD_ID] = (float)row.id;

        // Get transform component and write position data
        const components::TransformComponent* t = components::g_transform_components.GetComponent(row.id);
        if (t && std::isfinite(t->grid_x) && std::isfinite(t->grid_y) && std::isfinite((float)t->floor_z)) {
            // Convert grid coordinates to world coordinates
            float world_x = t->grid_x * 64.0f;
            float world_y = t->grid_y * 64.0f;
            float world_z = (float)t->floor_z;
            
            if (std::isfinite(world_x) && std::isfinite(world_y) && std::isfinite(world_z)) {
                datap[base + FIELD_X] = world_x;
                datap[base + FIELD_Y] = world_y;
                datap[base + FIELD_Z] = world_z;
            }
        }
        
        // Set other fields (TODO: get from actual components)
        datap[base + FIELD_VX]    = 0.0f;  // TODO: velocity from physics
        datap[base + FIELD_VY]    = 0.0f;  // TODO: velocity from physics
        datap[base + FIELD_ANG]   = 0.0f;  // TODO: rotation from transform
        datap[base + FIELD_FLAGS] = 0.0f;  // TODO: entity flags
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

    // Rotate buffers: move 'curr' to 'prev', create a new 'curr'
    // Note: We don't destroy s_prev_snapshot here because Lua might still be using it
    // The buffer will be garbage collected when Lua releases its reference
    s_prev_snapshot = s_curr_snapshot;
    s_curr_snapshot = 0;
    CreateAndFillSnapshot(s_curr_snapshot);

    // Boot diagnostics: print first few ticks' summary
    if (s_debug_ticks_printed < 3) {
        float *datap = 0; uint32_t n = 0, comps = 0, stride = 0;
        if (s_curr_snapshot) {
            dmBuffer::GetStream(s_curr_snapshot, dmHashString64("data"), (void**)&datap, &n, &comps, &stride);
        }
        if (datap && n > 0) {
            dmLogInfo("SNAPSHOT tick=%u entities=%u", s_current_tick, n);
        } else {
            dmLogInfo("SNAPSHOT tick=%u entities=0", s_current_tick);
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

    // Test buffer creation first
    // extern void TestBufferCreation();
    // extern void DebugBufferIssue();
    // TestBufferCreation();
    // DebugBufferIssue();

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
