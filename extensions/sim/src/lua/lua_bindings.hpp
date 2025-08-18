#pragma once
struct lua_State;
namespace simcore {
void LuaRegister_Sim(lua_State* L);
void LuaRegister_World(lua_State* L);
void LuaRegister_Observer(lua_State* L);
void LuaRegister_Portal(lua_State* L);
inline void LuaRegister_All(lua_State* L){
    LuaRegister_Sim(L);
    LuaRegister_World(L);
    LuaRegister_Observer(L);
    LuaRegister_Portal(L);
}
} // namespace simcore
