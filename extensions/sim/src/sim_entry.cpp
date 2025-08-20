// Entry TU with extension registration and scheduler
#include <dmsdk/sdk.h>
#include "core/sim_time.hpp"
#include "activation/activation.hpp"
#include "world/world.hpp"
#include "world/entity.hpp"
#include "observer/observer.hpp"
#include "systems/portal_system.hpp"
#include "core/events.hpp"
#include "lua/lua_bindings.hpp"
#include "systems/extractor_system.hpp"
#include "components/component_registry.hpp"

namespace simcore {

static double g_hz = 60.0;

static void StepOneTick(float dt){
    RebuildActivationUnion();
    
    // Run all systems (movement now handled directly in Lua)
    Portal_Step((int32_t)(dt*1000.0), (int64_t)(NowSeconds()*1000.0));
    Extractor_Step(dt);         // Handle resource extraction
    
    // Consume events here later (golems etc.). For now we just clear each frame.
    Events_Clear();
}

static dmExtension::Result AppInitialize(dmExtension::AppParams*) { return dmExtension::RESULT_OK; }
static dmExtension::Result AppFinalize(dmExtension::AppParams*)   { return dmExtension::RESULT_OK; }

static dmExtension::Result Initialize(dmExtension::Params* params){
    TimeInit();
    Portal_Init();
    Extractor_Init();
    
    // Initialize new entity system (includes component system)
    InitializeEntitySystem();
    
    // Register Lua functions
    LuaRegister_All(params->m_L);
    
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params*) { return dmExtension::RESULT_OK; }

static dmExtension::Result Update(dmExtension::Params* ){
    FixedStep([](float dt){ StepOneTick(dt); }, 1.0 / g_hz);
    return dmExtension::RESULT_OK;
}

} // namespace simcore

DM_DECLARE_EXTENSION(sim, "sim",
    simcore::AppInitialize, simcore::AppFinalize,
    simcore::Initialize,    simcore::Update,
    0,
    simcore::Finalize)
