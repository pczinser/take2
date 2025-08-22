#include <dmsdk/sdk.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/message.h>

#include "../core/sim_time.hpp"
#include "../sim_entry.hpp"      // <- C API declared here
#include "../command_queue.hpp"  // <- Command types and enums
#include "lua_bindings.hpp"

namespace simcore {

// sim.register_listener(msg.url())
static int L_register_listener(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    dmMessage::URL* url = dmScript::CheckURL(L, 1);
    if (!url) return luaL_error(L, "register_listener(url): invalid url");
    SetListenerURL(*url);
    return 0;
}

// Legacy: sim.read_snapshot() -> curr | nil
static int L_read_snapshot(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);
    dmBuffer::HBuffer curr = GetSnapshotBuffer();
    if (!curr) { lua_pushnil(L); return 1; }
    // Lua should NOT own this buffer; C++ will free it next tick
    dmScript::LuaHBuffer luabuf(curr, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    return 1;
}

// New: sim.read_snapshots() -> prev, curr, alpha:number, tick:uint
static int L_read_snapshots(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 4);
    dmBuffer::HBuffer prev = GetSnapshotBufferPrevious();
    dmBuffer::HBuffer curr = GetSnapshotBufferCurrent();

    if (!prev) lua_pushnil(L);
    else { dmScript::LuaHBuffer lb(prev, dmScript::OWNER_C); dmScript::PushBuffer(L, lb); }

    if (!curr) lua_pushnil(L);
    else { dmScript::LuaHBuffer lb(curr, dmScript::OWNER_C); dmScript::PushBuffer(L, lb); }

    lua_pushnumber(L, GetLastAlpha());
    lua_pushinteger(L, GetCurrentTick());
    return 4;
}

static int L_get_tick(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushinteger(L, GetCurrentTick());
    return 1;
}

static int L_get_dt(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushnumber(L, GetFixedDt());
    return 1;
}

// Optional controls (debug)
static int L_set_rate(lua_State* L) {
    double hz = luaL_checknumber(L, 1);
    if (hz < 1.0) hz = 1.0;
    if (hz > 480.0) hz = 480.0;
    SetSimHz(hz);
    return 0;
}

static int L_pause(lua_State* L) {
    bool paused = lua_toboolean(L, 1) != 0;
    SetSimPaused(paused);
    return 0;
}

static int L_step_n(lua_State* L) {
    int n = (int)luaL_checkinteger(L, 1);
    if (n < 0) n = 0;
    StepSimNTicks(n);     // only acts if paused
    return 0;
}

// Entity prototype registration
static int L_register_entity_prototypes(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    // This should register the entity prototypes table with the C++ system
    // For now, we'll just log that it was called
    dmLogInfo("register_entity_prototypes called - entity prototypes registered");
    
    // TODO: Implement actual prototype registration in C++
    // This might involve parsing the Lua table and storing the prototypes
    // for use when spawning entities via commands
    
    return 0;
}

// Observer queries
static int L_get_observers(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);
    
    // Create a table to hold observer data
    lua_newtable(L);
    
    // For now, return an empty table (no observers available yet)
    // TODO: Implement actual observer query from C++ system
    // When an observer is available, add it to this table
    
    return 1;  // Return the observers table (empty for now)
}

// Command queue bindings - typed functions
static int L_cmd_move_entity(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    
    Command cmd(CMD_MOVE_ENTITY, entity_id, 0, 0, dx, dy, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_spawn_entity(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    dmhash_t archetype = dmScript::CheckHash(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_optnumber(L, 4, 0.0);
    
    Command cmd(CMD_SPAWN_ENTITY, 0, (uint64_t)archetype, 0, x, y, z);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_destroy_entity(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    
    Command cmd(CMD_DESTROY_ENTITY, entity_id, 0, 0, 0.0f, 0.0f, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_set_entity_position(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    
    Command cmd(CMD_SET_ENTITY_POSITION, entity_id, 0, 0, x, y, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_set_entity_floor(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    int32_t floor = (int32_t)luaL_checkinteger(L, 2);
    
    Command cmd(CMD_SET_ENTITY_FLOOR, entity_id, 0, 0, 0.0f, 0.0f, (float)floor);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_add_item_to_inventory(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t slot = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t item_type = (uint32_t)luaL_checkinteger(L, 3);
    uint32_t quantity = (uint32_t)luaL_checkinteger(L, 4);
    
    Command cmd(CMD_ADD_ITEM_TO_INVENTORY, entity_id, slot, item_type, (float)quantity, 0.0f, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_remove_item_from_inventory(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t slot = (uint32_t)luaL_checkinteger(L, 2);
    uint32_t quantity = (uint32_t)luaL_checkinteger(L, 3);
    
    Command cmd(CMD_REMOVE_ITEM_FROM_INVENTORY, entity_id, slot, 0, (float)quantity, 0.0f, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_set_observer_position(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t observer_id = (uint32_t)luaL_checkinteger(L, 1);
    int32_t floor = (int32_t)luaL_checkinteger(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    
    Command cmd(CMD_SET_OBSERVER_POSITION, observer_id, 0, 0, x, y, (float)floor);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_set_entity_state_flag(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    dmhash_t flag = dmScript::CheckHash(L, 2);
    dmhash_t value = dmScript::CheckHash(L, 3);
    
    Command cmd(CMD_SET_ENTITY_STATE_FLAG, entity_id, (uint64_t)flag, (uint64_t)value, 0.0f, 0.0f, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_spawn_floor_at_z(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    
    int32_t z = (int32_t)luaL_checkinteger(L, 1);
    float start_x = (float)luaL_checknumber(L, 2);
    float start_y = (float)luaL_checknumber(L, 3);
    uint32_t width = (uint32_t)luaL_checkinteger(L, 4);
    uint32_t height = (uint32_t)luaL_checkinteger(L, 5);
    
    Command cmd(CMD_SPAWN_FLOOR_AT_Z, 0, width, height, start_x, start_y, (float)z);
    EnqueueCommand(cmd);
    return 0;
}

static int L_cmd_observer_follow_entity(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 0);
    uint32_t entity_id = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t observer_id = (uint32_t)luaL_optinteger(L, 2, 0);
    Command cmd(CMD_OBSERVER_FOLLOW_ENTITY, entity_id, observer_id, 0, 0.0f, 0.0f, 0.0f);
    EnqueueCommand(cmd);
    return 0;
}

static int L_get_stats(lua_State* L) {
    DM_LUA_STACK_CHECK(L, 1);
    lua_newtable(L);
    // ... your existing stats filling ...
    return 1;
}

void LuaRegister_Sim(lua_State* L) {
    static const luaL_reg API[] = {
        {"register_listener", L_register_listener},
        {"read_snapshot",     L_read_snapshot},    // legacy
        {"read_snapshots",    L_read_snapshots},   // prev+curr+alpha+tick
        {"get_tick",          L_get_tick},
        {"get_dt",            L_get_dt},
        {"set_rate",          L_set_rate},         // optional
        {"pause",             L_pause},            // optional
        {"step_n",            L_step_n},           // optional
        {"register_entity_prototypes", L_register_entity_prototypes}, // entity system setup
        {"get_observers",     L_get_observers},    // observer queries
        {"get_stats",         L_get_stats},
        // Command queue functions
        {"cmd_move_entity",           L_cmd_move_entity},
        {"cmd_spawn_entity",          L_cmd_spawn_entity},
        {"cmd_destroy_entity",        L_cmd_destroy_entity},
        {"cmd_set_entity_position",   L_cmd_set_entity_position},
        {"cmd_set_entity_floor",      L_cmd_set_entity_floor},
        {"cmd_add_item_to_inventory", L_cmd_add_item_to_inventory},
        {"cmd_remove_item_from_inventory", L_cmd_remove_item_from_inventory},
        {"cmd_set_observer_position", L_cmd_set_observer_position},
        {"cmd_set_entity_state_flag", L_cmd_set_entity_state_flag},
        {"cmd_spawn_floor_at_z", L_cmd_spawn_floor_at_z},
        {"cmd_observer_follow_entity", L_cmd_observer_follow_entity},
        {0, 0}
    };

    int top = lua_gettop(L);
    lua_getglobal(L, "sim");
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
    luaL_register(L, 0, API);
    lua_setglobal(L, "sim");
    (void)top;
}

} // namespace simcore
