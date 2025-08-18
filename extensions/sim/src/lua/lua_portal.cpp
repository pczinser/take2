#include <dmsdk/sdk.h>
#include "../systems/portal_system.hpp"
#include "lua_bindings.hpp"
namespace simcore {
static int L_add_portal(lua_State* L){
  PortalDesc d;
  d.from_z=(int16_t)luaL_checkinteger(L,1);
  d.from_cx=(int16_t)luaL_checkinteger(L,2);
  d.from_cy=(int16_t)luaL_checkinteger(L,3);
  d.from_tx=(int16_t)luaL_checkinteger(L,4);
  d.from_ty=(int16_t)luaL_checkinteger(L,5);
  d.to_z=(int16_t)luaL_checkinteger(L,6);
  d.to_cx=(int16_t)luaL_checkinteger(L,7);
  d.to_cy=(int16_t)luaL_checkinteger(L,8);
  d.to_tx=(int16_t)luaL_checkinteger(L,9);
  d.to_ty=(int16_t)luaL_checkinteger(L,10);
  d.cooldown_ms=(int32_t)luaL_optinteger(L,11,0);
  d.capacity=(int32_t)luaL_optinteger(L,12,0);
  lua_pushinteger(L, Portal_Add(d)); return 1;
}
static int L_portal_request(lua_State* L){
  PortalRequest r;
  r.sys=(uint8_t)luaL_checkinteger(L,1);
  r.id=(int32_t)luaL_checkinteger(L,2);
  r.z=(int16_t)luaL_checkinteger(L,3);
  r.cx=(int16_t)luaL_checkinteger(L,4);
  r.cy=(int16_t)luaL_checkinteger(L,5);
  r.tx=(int16_t)luaL_checkinteger(L,6);
  r.ty=(int16_t)luaL_checkinteger(L,7);
  Portal_Request(r); return 0;
}
void LuaRegister_Portal(lua_State* L){
  static const luaL_reg API[]={{"add_portal",L_add_portal},{"portal_request",L_portal_request},{0,0}};
  int top=lua_gettop(L); lua_getglobal(L,"sim"); if(lua_isnil(L,-1)){ lua_pop(L,1); lua_newtable(L); }
  luaL_register(L,0,API); lua_setglobal(L,"sim"); (void)top;
}
} // namespace simcore
