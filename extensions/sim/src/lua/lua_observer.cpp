#include <dmsdk/sdk.h>
#include "../observer/observer.hpp"
#include "lua_bindings.hpp"
namespace simcore {
static int L_set_observer(lua_State* L){
    int32_t z=(int32_t)luaL_checkinteger(L,1);
    int32_t tx=(int32_t)luaL_checkinteger(L,2);
    int32_t ty=(int32_t)luaL_checkinteger(L,3);
    int32_t hot=(int32_t)luaL_checkinteger(L,4);
    int32_t warm=(int32_t)luaL_checkinteger(L,5);
    int32_t hotz=(int32_t)luaL_optinteger(L,6,0);
    int32_t warmz=(int32_t)luaL_optinteger(L,7,1);
    lua_pushinteger(L, SetObserver(z,tx,ty,hot,warm,hotz,warmz)); return 1;
}
static int L_move_observer(lua_State* L){
    int32_t id=(int32_t)luaL_checkinteger(L,1);
    int32_t z=(int32_t)luaL_checkinteger(L,2);
    int32_t tx=(int32_t)luaL_checkinteger(L,3);
    int32_t ty=(int32_t)luaL_checkinteger(L,4);
    MoveObserver(id,z,tx,ty); return 0;
}
void LuaRegister_Observer(lua_State* L){
    static const luaL_reg API[]={{"set_observer",L_set_observer},{"move_observer",L_move_observer},{0,0}};
    int top=lua_gettop(L); lua_getglobal(L,"sim"); if(lua_isnil(L,-1)){ lua_pop(L,1); lua_newtable(L); }
    luaL_register(L,0,API); lua_setglobal(L,"sim"); (void)top;
}
} // namespace simcore
