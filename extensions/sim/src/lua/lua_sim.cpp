#include <dmsdk/sdk.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/message.h>

#include "../core/sim_time.hpp"
#include "../sim_entry.hpp"      // <- C API declared here
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
        {"get_stats",         L_get_stats},
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
