#include <dmsdk/sdk.h>
#include "../core/sim_time.hpp"
#include "../activation/activation.hpp"
#include "../world/world.hpp"
#include "../observer/observer.hpp"
#include "../systems/portal_system.hpp"
#include "../core/events.hpp"
#include "lua_bindings.hpp"
#include "../systems/inventory_system.hpp"

namespace simcore {
static bool s_paused=false; static double s_hz=60.0;

static int L_set_rate(lua_State* L){ double hz=luaL_checknumber(L,1); if(hz<1.0) hz=1.0; if(hz>480.0) hz=480.0; s_hz=hz; return 0; }
static int L_pause(lua_State* L){ s_paused = lua_toboolean(L,1)!=0; return 0; }
static int L_step_n(lua_State* L){
    int n=(int)luaL_checkinteger(L,1);
    for(int i=0;i<n;++i){ RebuildActivationUnion(); Portal_Step(16,(int64_t)(NowSeconds()*1000.0)); }
    return 0;
}
static int L_get_stats(lua_State* L){
    DM_LUA_STACK_CHECK(L,1);
    lua_newtable(L);
    lua_pushinteger(L,(lua_Integer)GetFloorZList().size()); lua_setfield(L,-2,"floors");
    lua_pushinteger(L,(lua_Integer)GetObservers().size());  lua_setfield(L,-2,"observers");
    lua_pushinteger(L,(lua_Integer)Portal_GetStats().count); lua_setfield(L,-2,"portals");
    lua_newtable(L); int idx=1;
    for(int32_t z: GetFloorZList()){
        auto* f=GetFloorByZ(z); if(!f) continue;
        lua_createtable(L,0,5);
        lua_pushinteger(L,z); lua_setfield(L,-2,"z");
        lua_pushinteger(L,f->chunks_w); lua_setfield(L,-2,"chunks_w");
        lua_pushinteger(L,f->chunks_h); lua_setfield(L,-2,"chunks_h");
        lua_pushinteger(L,(lua_Integer)f->hot_chunks.size());  lua_setfield(L,-2,"hot");
        lua_pushinteger(L,(lua_Integer)f->warm_chunks.size()); lua_setfield(L,-2,"warm");
        lua_rawseti(L,-2,idx++);
    }
    lua_setfield(L,-2,"floor_stats");
    return 1;
}

void LuaRegister_Sim(lua_State* L){
    static const luaL_reg API[]={
        {"set_rate",L_set_rate},
        {"pause",L_pause},
        {"step_n",L_step_n},
        {"get_stats",L_get_stats},
        {0,0}
    };
    int top=lua_gettop(L);
    lua_getglobal(L,"sim"); if(lua_isnil(L,-1)){ lua_pop(L,1); lua_newtable(L); }
    luaL_register(L,0,API); lua_setglobal(L,"sim"); (void)top;
}
} // namespace simcore
