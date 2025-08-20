#include <dmsdk/sdk.h>
#include "../world/world.hpp"
#include "lua_bindings.hpp"
#include "../world/entity.hpp"
#include "../components/component_registry.hpp"
#include "../observer/observer.hpp"
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
    const char* prototype_name = luaL_checkstring(L, 1);
    float grid_x = (float)luaL_checknumber(L, 2);
    float grid_y = (float)luaL_checknumber(L, 3);
    int32_t floor_z = (int32_t)luaL_optinteger(L, 4, 0);
    
    // Create entity from prototype
    EntityId entity_id = CreateEntity(prototype_name, grid_x, grid_y, floor_z);
    lua_pushinteger(L, entity_id);
    return 1;
}

static int L_get_entity(lua_State* L) {
    EntityId id = (EntityId)luaL_checkinteger(L, 1);
    
    Entity* entity = GetEntity(id);
    if (!entity) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushinteger(L, entity->id);
    lua_setfield(L, -2, "id");
    
    lua_pushstring(L, entity->name.c_str());
    lua_setfield(L, -2, "name");
    
    lua_pushstring(L, entity->prototype_name.c_str());  // ← Fixed: template_name → prototype_name
    lua_setfield(L, -2, "prototype_name");  // ← Fixed: template_name → prototype_name
    
    lua_pushboolean(L, entity->is_dirty);
    lua_setfield(L, -2, "is_dirty");
    
    // Add transform component data
    components::TransformComponent* transform = components::g_transform_components.GetComponent(id);
    if (transform) {
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
        lua_pushnumber(L, transform->move_speed);
        lua_setfield(L, -2, "move_speed");
    }
    
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

static int L_move_entity(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    float dx = (float)luaL_checknumber(L, 2);
    float dy = (float)luaL_checknumber(L, 3);
    
    // Simple direct movement using transform component
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (transform) {
        float new_x = transform->grid_x + dx;
        float new_y = transform->grid_y + dy;
        
        // Store old position for chunk mapping
        int32_t old_chunk_x = transform->chunk_x;
        int32_t old_chunk_y = transform->chunk_y;
        int32_t old_floor_z = transform->floor_z;
        
        // Update position
        transform->grid_x = new_x;
        transform->grid_y = new_y;
        transform->chunk_x = (int32_t)(new_x / 32.0f);
        transform->chunk_y = (int32_t)(new_y / 32.0f);
        
        // Mark entity as dirty for rendering
        Entity* entity = GetEntity(entity_id);
        if (entity) {
            entity->is_dirty = true;
        }
        
        // Update chunk mapping if needed
        if (old_chunk_x != transform->chunk_x || old_chunk_y != transform->chunk_y) {
            UpdateEntityChunkMapping(entity_id, old_chunk_x, old_chunk_y, old_floor_z);
        }
        
        printf("Entity %d moved to (%.1f, %.1f)\n", entity_id, new_x, new_y);
    }
    
    return 0;
}

static int L_set_entity_position(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    float grid_x = (float)luaL_checknumber(L, 2);
    float grid_y = (float)luaL_checknumber(L, 3);
    
    // Direct position setting (teleport)
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (transform) {
        // Store old position for chunk mapping
        int32_t old_chunk_x = transform->chunk_x;
        int32_t old_chunk_y = transform->chunk_y;
        int32_t old_floor_z = transform->floor_z;
        
        // Update position directly
        transform->grid_x = grid_x;
        transform->grid_y = grid_y;
        transform->chunk_x = (int32_t)(grid_x / 32.0f);
        transform->chunk_y = (int32_t)(grid_y / 32.0f);
        
        // Mark entity as dirty for rendering
        Entity* entity = GetEntity(entity_id);
        if (entity) {
            entity->is_dirty = true;
        }
        
        // Update chunk mapping if needed
        if (old_chunk_x != transform->chunk_x || old_chunk_y != transform->chunk_y) {
            UpdateEntityChunkMapping(entity_id, old_chunk_x, old_chunk_y, old_floor_z);
        }
        
        printf("Entity %d teleported to (%.1f, %.1f)\n", entity_id, grid_x, grid_y);
    }
    
    return 0;
}

static int L_set_entity_floor(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 2);
    
    // Direct floor change using transform component
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (transform) {
        int32_t old_floor_z = transform->floor_z;
        transform->floor_z = floor_z;
        
        // Ensure target floor exists
        Floor* floor = GetFloorByZ(floor_z);
        if (!floor) {
            int32_t chunks_w = (floor_z > 0) ? 2 : 4;
            int32_t chunks_h = (floor_z > 0) ? 2 : 4;
            SpawnFloorAtZ(floor_z, chunks_w, chunks_h, 32, 32);
            printf("Auto-created floor %d for entity movement\n", floor_z);
        }
        
        // Mark entity as dirty for rendering
        Entity* entity = GetEntity(entity_id);
        if (entity) {
            entity->is_dirty = true;
        }
        
        // Update chunk mapping (floor changed)
        UpdateEntityChunkMapping(entity_id, transform->chunk_x, transform->chunk_y, old_floor_z);
        
        printf("Entity %d changed from floor %d to floor %d\n", entity_id, old_floor_z, floor_z);
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

static int L_get_entities_on_floor(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    auto entities = GetEntitiesOnFloor(floor_z);
    
    lua_newtable(L);
    for(int i = 0; i < (int)entities.size(); i++) {
        lua_pushinteger(L, entities[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Add this function BEFORE the L_register_entity_prototypes function (around line 310)

static void CreateComponentInstancesFromLua(EntityId entity_id, const std::string& prototype_name, lua_State* L, int prototype_index, float grid_x, float grid_y, int32_t floor_z) {
    // Simple implementation - just create basic components for now
    
    // Create metadata component
    components::g_metadata_components.AddComponent(entity_id, 
        components::MetadataComponent(prototype_name, "entity", ""));
    
    // Create transform component
    components::g_transform_components.AddComponent(entity_id, 
        components::TransformComponent(grid_x, grid_y, floor_z, 100.0f));
    
    printf("Created basic components for entity %d from prototype %s\n", entity_id, prototype_name.c_str());
}

// === PROTOTYPE REGISTRATION ===

static int L_register_entity_prototypes(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "Expected table of entity prototypes");
    }
    
    lua_pushnil(L);  // First key
    while (lua_next(L, 1) != 0) {
        // Key is at index -2, value is at index -1
        if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
            lua_pop(L, 1);  // Remove value, keep key
            continue;
        }
        
        const char* prototype_name = lua_tostring(L, -2);
        
        // Create prototype entity manually (since it's not a real simulation entity)
        EntityId prototype_id = GetNextEntityId();
        Entity prototype_entity(prototype_id, prototype_name, prototype_name);
        AddEntityToList(prototype_entity);
        
        // Parse Lua template and create components for prototype
        CreateComponentInstancesFromLua(prototype_id, prototype_name, L, -1, 0, 0, 0);
        
        // Register the prototype
        RegisterEntityPrototype(prototype_name, prototype_id);
        
        lua_pop(L, 1);  // Remove template value, keep key for next iteration
    }
    
    return 0;
}

// === OBSERVER SYSTEM ===

static int L_set_observer(lua_State* L) {
    int32_t z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tx = (int32_t)luaL_checkinteger(L, 2);
    int32_t ty = (int32_t)luaL_checkinteger(L, 3);
    int32_t hot = (int32_t)luaL_checkinteger(L, 4);
    int32_t warm = (int32_t)luaL_checkinteger(L, 5);
    int32_t hotz = (int32_t)luaL_optinteger(L, 6, 0);
    int32_t warmz = (int32_t)luaL_optinteger(L, 7, 1);
    
    lua_pushinteger(L, SetObserver(z, tx, ty, hot, warm, hotz, warmz));
    return 1;
}

static int L_move_observer(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t z = (int32_t)luaL_checkinteger(L, 2);
    int32_t tx = (int32_t)luaL_checkinteger(L, 3);
    int32_t ty = (int32_t)luaL_checkinteger(L, 4);
    
    MoveObserver(id, z, tx, ty);
    return 0;
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
    {"set_entity_position", L_set_entity_position},
    {"set_entity_floor", L_set_entity_floor},
    
    // Component access functions
    {"get_entity_metadata", L_get_entity_metadata},
    {"get_entity_transform", L_get_entity_transform},
    
    // Spatial query functions
    {"get_entities_in_chunk", L_get_entities_in_chunk},
    {"get_entities_in_radius", L_get_entities_in_radius},
    {"get_entities_at_tile", L_get_entities_at_tile},
    {"get_entities_on_floor", L_get_entities_on_floor},
    
    // Prototype registration
    {"register_entity_prototypes", L_register_entity_prototypes},
    
    // Observer system
    {"set_observer", L_set_observer},
    {"move_observer", L_move_observer},
    
    // Floor management
    {"set_current_floor", L_set_current_floor},
    {"get_current_floor", L_get_current_floor},
    
    {NULL, NULL}
};

} // namespace simcore

// Lua registration function (called from sim_entry.cpp)
void simcore::LuaRegister_World(lua_State* L) {
    luaL_register(L, "sim", simcore::sim_functions);
}