#include <dmsdk/sdk.h>
#include "../world/world.hpp"
#include "lua_bindings.hpp"
#include "../world/entity_new.hpp"
#include "../components/component_registry.hpp"
#include "../systems/inventory_system.hpp"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>

namespace simcore {

// === WORLD FUNCTIONS ===
static int L_spawn_floor_at_z(lua_State* L){
    int32_t z=(int32_t)luaL_checkinteger(L,1);
    int32_t cw=(int32_t)luaL_checkinteger(L,2);
    int32_t ch=(int32_t)luaL_checkinteger(L,3);
    int32_t tw=(int32_t)luaL_checkinteger(L,4);
    int32_t th=(int32_t)luaL_checkinteger(L,5);
    lua_pushinteger(L, SpawnFloorAtZ(z,cw,ch,tw,th)); return 1;
}

static int L_get_active_chunks(lua_State* L){
    int32_t z=(int32_t)luaL_checkinteger(L,1);
    Floor* f=GetFloorByZ(z); lua_newtable(L); if(!f) return 1;
    lua_newtable(L); int i=1;
    for(auto key:f->hot_chunks){ int16_t cx=(int16_t)(key>>32), cy=(int16_t)(key&0xffffffff);
        lua_createtable(L,2,0); lua_pushinteger(L,cx); lua_rawseti(L,-2,1); lua_pushinteger(L,cy); lua_rawseti(L,-2,2); lua_rawseti(L,-2,i++); }
    lua_setfield(L,-2,"hot");
    lua_newtable(L); i=1;
    for(auto key:f->warm_chunks){ int16_t cx=(int16_t)(key>>32), cy=(int16_t)(key&0xffffffff);
        lua_createtable(L,2,0); lua_pushinteger(L,cx); lua_rawseti(L,-2,1); lua_pushinteger(L,cy); lua_rawseti(L,-2,2); lua_rawseti(L,-2,i++); }
    lua_setfield(L,-2,"warm"); return 1;
}

// === ENTITY FUNCTIONS ===

static int L_create_entity(lua_State* L) {
    const char* template_name = luaL_checkstring(L, 1);
    float grid_x = (float)luaL_checknumber(L, 2);
    float grid_y = (float)luaL_checknumber(L, 3);
    int32_t floor_z = (int32_t)luaL_optinteger(L, 4, 0);
    
    EntityId entity_id = CreateEntity(template_name, grid_x, grid_y, floor_z);
    lua_pushinteger(L, entity_id);
    return 1;
}

static int L_get_entity(lua_State* L) {
    EntityId id = (EntityId)luaL_checkinteger(L, 1);
    Entity* entity = GetEntity(id);
    
    if(!entity) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushinteger(L, entity->id);
    lua_setfield(L, -2, "id");
    
    lua_pushstring(L, entity->name.c_str());
    lua_setfield(L, -2, "name");
    
    lua_pushstring(L, entity->template_name.c_str());
    lua_setfield(L, -2, "template_name");
    
    lua_pushboolean(L, entity->is_dirty);
    lua_setfield(L, -2, "is_dirty");
    
    return 1;
}

static int L_destroy_entity(lua_State* L) {
    EntityId id = (EntityId)luaL_checkinteger(L, 1);
    DestroyEntity(id);
    return 0;
}

// === COMPONENT ACCESS FUNCTIONS ===

static int L_get_entity_metadata(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    components::MetadataComponent* metadata = components::g_metadata_components.GetComponent(entity_id);
    if (!metadata) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushstring(L, metadata->display_name.c_str());
    lua_setfield(L, -2, "display_name");
    
    return 1;
}

static int L_get_entity_transform(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (!transform) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushnumber(L, transform->grid_x);
    lua_setfield(L, -2, "grid_x");
    lua_pushnumber(L, transform->grid_y);
    lua_setfield(L, -2, "grid_y");
    lua_pushinteger(L, transform->floor_z);
    lua_setfield(L, -2, "floor_z");
    lua_pushinteger(L, transform->chunk_x);
    lua_setfield(L, -2, "chunk_x");
    lua_pushinteger(L, transform->chunk_y);
    lua_setfield(L, -2, "chunk_y");
    
    return 1;
}

static int L_get_entity_movement(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (!movement) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushnumber(L, movement->move_speed);
    lua_setfield(L, -2, "move_speed");
    lua_pushnumber(L, movement->current_dx);
    lua_setfield(L, -2, "current_dx");
    lua_pushnumber(L, movement->current_dy);
    lua_setfield(L, -2, "current_dy");
    
    return 1;
}

static int L_move_entity(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (transform) {
        // Update position
        float new_x = transform->grid_x + dx;
        float new_y = transform->grid_y + dy;
        
        // Update chunk coordinates
        int32_t old_chunk_x = transform->chunk_x;
        int32_t old_chunk_y = transform->chunk_y;
        int32_t new_chunk_x = (int32_t)(new_x / 32.0f);
        int32_t new_chunk_y = (int32_t)(new_y / 32.0f);
        
        transform->grid_x = new_x;
        transform->grid_y = new_y;
        transform->chunk_x = new_chunk_x;
        transform->chunk_y = new_chunk_y;
        
        // Mark entity as dirty for rendering
        Entity* entity = GetEntity(entity_id);
        if (entity) {
            entity->is_dirty = true;
        }
        
        if(old_chunk_x != new_chunk_x || old_chunk_y != new_chunk_y) {
            printf("Entity %d moved from chunk (%d, %d) to (%d, %d)\n", 
                   entity_id, old_chunk_x, old_chunk_y, new_chunk_x, new_chunk_y);
        }
    }
    
    return 0;
}

// === SPATIAL QUERY FUNCTIONS ===

static int L_get_entities_in_chunk(lua_State* L) {
    int32_t z = (int32_t)luaL_checkinteger(L, 1);
    int32_t chunk_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t chunk_y = (int32_t)luaL_checkinteger(L, 3);
    
    auto entities = GetEntitiesInChunk(z, chunk_x, chunk_y);
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_get_entities_in_radius(lua_State* L) {
    float grid_x = (float)luaL_checknumber(L, 1);
    float grid_y = (float)luaL_checknumber(L, 2);
    float radius = (float)luaL_checknumber(L, 3);
    
    auto entities = GetEntitiesInRadius(grid_x, grid_y, radius);
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_get_entities_at_tile(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 3);
    
    auto entities = GetEntitiesAtTile(floor_z, tile_x, tile_y);
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

// === TEMPLATE REGISTRATION ===

static int L_register_entity_templates(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected table of entity templates");
    }
    
    lua_pushnil(L);  // First key
    while (lua_next(L, 1) != 0) {
        // Key is at index -2, value is at index -1
        if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
            lua_pop(L, 1);  // Remove value, keep key
            continue;
        }
        
        const char* template_name = lua_tostring(L, -2);
        
        // Parse template from Lua table
        EntityTemplate templ;
        
        // Get display name (default to template name)
        lua_getfield(L, -1, "display_name");
        if (lua_isstring(L, -1)) {
            templ.display_name = lua_tostring(L, -1);
        } else {
            templ.display_name = template_name;
        }
        lua_pop(L, 1);
        
        // Get category
        lua_getfield(L, -1, "category");
        if (lua_isstring(L, -1)) {
            templ.category = lua_tostring(L, -1);
        } else {
            // Default category based on template name
            templ.category = "entity";
        }
        lua_pop(L, 1);
        
        // Parse components table
        lua_getfield(L, -1, "components");
        if (lua_istable(L, -1)) {
            // Parse metadata component
            lua_getfield(L, -1, "metadata");
            if (lua_istable(L, -1)) {
                templ.components.has_metadata = true;
                lua_getfield(L, -1, "display_name");
                if (lua_isstring(L, -1)) {
                    templ.components.metadata_display_name = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            
            // Parse movement component
            lua_getfield(L, -1, "movement");
            if (lua_istable(L, -1)) {
                templ.components.has_movement = true;
                lua_getfield(L, -1, "move_speed");
                if (lua_isnumber(L, -1)) {
                    templ.components.movement_speed = (float)lua_tonumber(L, -1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            
            // Parse building component
            lua_getfield(L, -1, "building");
            if (lua_istable(L, -1)) {
                templ.components.has_building = true;
                lua_getfield(L, -1, "width");
                if (lua_isnumber(L, -1)) {
                    templ.components.building_width = (int32_t)lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, -1, "height");
                if (lua_isnumber(L, -1)) {
                    templ.components.building_height = (int32_t)lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
                
                lua_getfield(L, -1, "building_type");
                if (lua_isstring(L, -1)) {
                    templ.components.building_type = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            
            // Parse inventory component
            lua_getfield(L, -1, "inventory");
            if (lua_istable(L, -1)) {
                templ.components.has_inventory = true;
                lua_getfield(L, -1, "slots");
                if (lua_istable(L, -1)) {
                    lua_pushnil(L);  // First key for slots iteration
                    while (lua_next(L, -2) != 0) {
                        if (lua_istable(L, -1)) {
                            EntityTemplate::ComponentData::InventorySlot slot;
                            lua_getfield(L, -1, "type");
                            if (lua_isnumber(L, -1)) {
                                slot.inventory_type = (int32_t)lua_tointeger(L, -1);
                            }
                            lua_pop(L, 1);
                            
                            lua_getfield(L, -1, "capacity");
                            if (lua_isnumber(L, -1)) {
                                slot.capacity = (int32_t)lua_tointeger(L, -1);
                            }
                            lua_pop(L, 1);
                            
                            templ.components.inventory_slots.push_back(slot);
                        }
                        lua_pop(L, 1);  // Remove value, keep key
                    }
                }
                lua_pop(L, 1);  // Pop slots table
            }
            lua_pop(L, 1);  // Pop inventory table
        }
        lua_pop(L, 1);  // Pop components table
        
        RegisterEntityTemplate(template_name, templ);
        lua_pop(L, 1);  // Remove template value, keep key for next iteration
    }
    
    return 0;
}

// === INVENTORY SYSTEM BINDINGS ===

static int L_inventory_create(lua_State* L) {
    int32_t type = (int32_t)luaL_checkinteger(L, 1);
    int32_t capacity = (int32_t)luaL_checkinteger(L, 2);
    
    InventoryId inv_id = Inventory_Create((InventoryType)type, capacity);
    lua_pushinteger(L, inv_id);
    return 1;
}

static int L_inventory_add_items(lua_State* L) {
    InventoryId inv_id = (InventoryId)luaL_checkinteger(L, 1);
    int32_t item_type = (int32_t)luaL_checkinteger(L, 2);
    int32_t amount = (int32_t)luaL_checkinteger(L, 3);
    
    bool success = Inventory_AddItems(inv_id, (ItemType)item_type, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_get_item_count(lua_State* L) {
    InventoryId inv_id = (InventoryId)luaL_checkinteger(L, 1);
    int32_t item_type = (int32_t)luaL_checkinteger(L, 2);
    
    int32_t count = Inventory_GetItemCount(inv_id, (ItemType)item_type);
    lua_pushinteger(L, count);
    return 1;
}

// === FLOOR MANAGEMENT ===

static int L_set_current_floor(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    SetCurrentFloor(floor_z);
    return 0;
}

static int L_get_current_floor(lua_State* L) {
    lua_pushinteger(L, GetCurrentFloor());
    return 1;
}

// === FUNCTION REGISTRATION ===

static const luaL_Reg sim_functions[] = {
    // World functions
    {"spawn_floor_at_z", L_spawn_floor_at_z},
    {"get_active_chunks", L_get_active_chunks},
    
    // Entity functions
    {"create_entity", L_create_entity},
    {"get_entity", L_get_entity},
    {"destroy_entity", L_destroy_entity},
    {"move_entity", L_move_entity},
    
    // Component access functions
    {"get_entity_metadata", L_get_entity_metadata},
    {"get_entity_transform", L_get_entity_transform},
    {"get_entity_movement", L_get_entity_movement},
    
    // Spatial query functions
    {"get_entities_in_chunk", L_get_entities_in_chunk},
    {"get_entities_in_radius", L_get_entities_in_radius},
    {"get_entities_at_tile", L_get_entities_at_tile},
    
    // Template registration
    {"register_entity_templates", L_register_entity_templates},
    
    // Inventory functions
    {"inventory_create", L_inventory_create},
    {"inventory_add_items", L_inventory_add_items},
    {"inventory_get_item_count", L_inventory_get_item_count},
    
    // Floor management
    {"set_current_floor", L_set_current_floor},
    {"get_current_floor", L_get_current_floor},
    
    {NULL, NULL}
};

} // namespace simcore

// Register the extension
static int LuaInit(lua_State* L) {
    int top = lua_gettop(L);
    
    luaL_register(L, "sim", simcore::sim_functions);
    
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return 0;
}

static dmExtension::Result AppInitialize(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initialize(dmExtension::Params* params) {
    LuaInit(params->m_L);
    
    // Initialize systems
    simcore::InitializeEntitySystem();
    simcore::Inventory_Init();
    
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalize(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params* params) {
    simcore::ClearEntitySystem();
    simcore::Inventory_Clear();
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(sim, "sim", AppInitialize, AppFinalize, Initialize, Finalize, 0, 0)