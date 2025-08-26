#include <dmsdk/sdk.h>
#include "../world/world.hpp"
#include "lua_bindings.hpp"
#include "../world/entity.hpp"
#include "../components/component_registry.hpp"
#include "../observer/observer.hpp"
#include "../systems/inventory_system.hpp"
#include "../items.hpp"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>
#include <dmsdk/dlib/hash.h>

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

static int L_get_entity_visual_config(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    components::VisualComponent* visual = components::g_visual_components.GetComponent(entity_id);
    if (!visual) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    lua_pushstring(L, visual->atlas_path.c_str());
    lua_setfield(L, -2, "atlas_path");
    
    lua_pushinteger(L, visual->layer);
    lua_setfield(L, -2, "layer");
    
    // Create animations table
    lua_newtable(L);
    for (const auto& anim : visual->animations) {
        lua_newtable(L);
        
        // Create conditions table
        lua_newtable(L);
        for (const auto& [key, value] : anim.conditions) {
            lua_pushstring(L, key.c_str());
            lua_pushstring(L, value.c_str());
            lua_settable(L, -3);
        }
        lua_setfield(L, -2, "conditions");
        
        lua_setfield(L, -2, anim.name.c_str());
    }
    lua_setfield(L, -2, "animations");
    
    return 1;
}

static int L_get_entity_animation_state(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    components::AnimStateComponent* anim_state = components::g_animstate_components.GetComponent(entity_id);
    if (!anim_state) {
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    for (const auto& [key, value] : anim_state->conditions) {
        lua_pushstring(L, key.c_str());
        lua_pushstring(L, value.c_str());
        lua_settable(L, -3);
    }
    
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
    lua_pushnumber(L, transform->grid_x); lua_setfield(L, -2, "grid_x");
    lua_pushnumber(L, transform->grid_y); lua_setfield(L, -2, "grid_y");
    lua_pushinteger(L, transform->floor_z); lua_setfield(L, -2, "floor_z");
    lua_pushnumber(L, transform->move_speed); lua_setfield(L, -2, "move_speed");
    lua_pushinteger(L, transform->width); lua_setfield(L, -2, "width");
    lua_pushinteger(L, transform->height); lua_setfield(L, -2, "height");
    lua_pushstring(L, transform->facing.c_str()); lua_setfield(L, -2, "facing");
    
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
    // Get the components table
    lua_getfield(L, prototype_index, "components");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    
    // Parse metadata component
    lua_getfield(L, -1, "metadata");
    if (lua_istable(L, -1)) {
        std::string display_name = prototype_name;
        std::string category = "entity";
        std::string description = "";
        
        lua_getfield(L, -1, "display_name");
        if (lua_isstring(L, -1)) {
            display_name = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "category");
        if (lua_isstring(L, -1)) {
            category = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        
        components::g_metadata_components.AddComponent(entity_id, 
            components::MetadataComponent(display_name, category, description));
    }
    lua_pop(L, 1);
    
    // Parse transform component (now includes size)
    lua_getfield(L, -1, "transform");
    if (lua_istable(L, -1)) {
        float move_speed = 100.0f;
        int32_t width = 1, height = 1;
        
        lua_getfield(L, -1, "move_speed");
        if (lua_isnumber(L, -1)) move_speed = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "width");
        if (lua_isnumber(L, -1)) width = (int32_t)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "height");
        if (lua_isnumber(L, -1)) height = (int32_t)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        components::g_transform_components.AddComponent(entity_id, 
            components::TransformComponent(grid_x, grid_y, floor_z, move_speed, width, height));
    } else {
        // Default transform if not specified
        components::g_transform_components.AddComponent(entity_id, 
            components::TransformComponent(grid_x, grid_y, floor_z, 100.0f, 1, 1));
    }
    lua_pop(L, 1);
    
    // Remove building component parsing entirely - size is now in transform component
    
    // Parse production component
    lua_getfield(L, -1, "production");
    if (lua_istable(L, -1)) {
        float production_rate = 0.0f;
        float extraction_rate = 0.0f;
        int32_t target_resource = -1;
        
        lua_getfield(L, -1, "production_rate");
        if (lua_isnumber(L, -1)) production_rate = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "extraction_rate");
        if (lua_isnumber(L, -1)) extraction_rate = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "target_resource");
        if (lua_isnumber(L, -1)) target_resource = (int32_t)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        components::ProductionComponent prod_comp;
        prod_comp.production_rate = production_rate;
        prod_comp.extraction_rate = extraction_rate;
        prod_comp.target_resource = target_resource;
        components::g_production_components.AddComponent(entity_id, prod_comp);
    }
    lua_pop(L, 1);
    
    // Parse health component
    lua_getfield(L, -1, "health");
    if (lua_istable(L, -1)) {
        int32_t current_health = 100;
        int32_t max_health = 100;
        
        lua_getfield(L, -1, "current_health");
        if (lua_isnumber(L, -1)) current_health = (int32_t)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "max_health");
        if (lua_isnumber(L, -1)) max_health = (int32_t)lua_tonumber(L, -1);
        lua_pop(L, 1);
        
        components::g_health_components.AddComponent(entity_id, 
            components::HealthComponent(current_health, max_health));
    }
    lua_pop(L, 1);
    
    // Parse inventory component
    lua_getfield(L, -1, "inventory");
    if (lua_istable(L, -1)) {
        std::vector<components::InventoryComponent::InventorySlot> slots;
        
        lua_getfield(L, -1, "slots");
        if (lua_istable(L, -1)) {
            int slot_count = lua_objlen(L, -1);
            for (int i = 1; i <= slot_count; i++) {
                lua_rawgeti(L, -1, i);
                if (lua_istable(L, -1)) {
                    components::InventoryComponent::InventorySlot slot;
                    
                    lua_getfield(L, -1, "is_output");
                    if (lua_isboolean(L, -1)) slot.is_output = lua_toboolean(L, -1);
                    lua_pop(L, 1);
                    
                    lua_getfield(L, -1, "whitelist");
                    if (lua_istable(L, -1)) {
                        int whitelist_count = lua_objlen(L, -1);
                        for (int j = 1; j <= whitelist_count; j++) {
                            lua_rawgeti(L, -1, j);
                            if (lua_isnumber(L, -1)) {
                                slot.whitelist.push_back((ItemType)lua_tonumber(L, -1));
                            }
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                    
                    slots.push_back(slot);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        
        components::g_inventory_components.AddComponent(entity_id, 
            components::InventoryComponent(slots));
    }
    lua_pop(L, 1);
    
    // Parse visual component
    lua_getfield(L, -1, "visual");
    if (lua_istable(L, -1)) {
        std::string atlas_path = "/asset/atlas/player.atlas";  // Default atlas
        int32_t layer = 1;
        
        lua_getfield(L, -1, "atlas_path");
        if (lua_isstring(L, -1)) {
            atlas_path = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
        
        lua_getfield(L, -1, "layer");
        if (lua_isnumber(L, -1)) {
            layer = (int32_t)lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
        
        components::VisualComponent visual_comp(atlas_path, layer);
        
        // Parse animations
        lua_getfield(L, -1, "animations");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);  // First key
            while (lua_next(L, -2) != 0) {
                // Key is at index -2, value is at index -1
                if (lua_isstring(L, -2) && lua_istable(L, -1)) {
                    std::string anim_name = lua_tostring(L, -2);
                    components::VisualComponent::AnimationCondition anim_cond(anim_name);
                    
                    // Parse conditions
                    lua_getfield(L, -1, "conditions");
                    if (lua_istable(L, -1)) {
                        lua_pushnil(L);  // First key
                        while (lua_next(L, -2) != 0) {
                            // Key is at index -2, value is at index -1
                            if (lua_isstring(L, -2)) {
                                std::string condition_key = lua_tostring(L, -2);
                                std::string condition_value;
                                
                                if (lua_isstring(L, -1)) {
                                    condition_value = lua_tostring(L, -1);
                                } else if (lua_isboolean(L, -1)) {
                                    condition_value = lua_toboolean(L, -1) ? "true" : "false";
                                } else if (lua_isnumber(L, -1)) {
                                    condition_value = std::to_string(lua_tonumber(L, -1));
                                }
                                
                                anim_cond.conditions[condition_key] = condition_value;
                            }
                            lua_pop(L, 1);  // Remove value, keep key for next iteration
                        }
                    }
                    lua_pop(L, 1);  // Pop conditions table
                    
                    visual_comp.animations.push_back(anim_cond);
                }
                lua_pop(L, 1);  // Remove value, keep key for next iteration
            }
        }
        lua_pop(L, 1);  // Pop animations table
        
        components::g_visual_components.AddComponent(entity_id, visual_comp);
    }
    lua_pop(L, 1);
    
    lua_pop(L, 1); // Pop components table
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
        
        // Allocate a deterministic prototype id from name-hash (separate from runtime entity ids)
        uint64_t h64 = dmHashString64(prototype_name);
        EntityId prototype_id = (EntityId)(h64 & 0x7fffffff); // keep in positive 31-bit range

        // Parse Lua template and create components for prototype (stored under prototype_id)
        CreateComponentInstancesFromLua(prototype_id, prototype_name, L, -1, 0, 0, 0);

        // Register the prototype in the catalog
        RegisterEntityPrototype(prototype_name, prototype_id);

        // Diagnostics: verify prototype component presence
        {
            auto t = components::g_transform_components.GetComponent(prototype_id) != nullptr;
            auto a = components::g_animstate_components.GetComponent(prototype_id) != nullptr;
            auto m = components::g_metadata_components.GetComponent(prototype_id) != nullptr;
            auto i = components::g_inventory_components.GetComponent(prototype_id) != nullptr;
            printf("PROTO OK name=%s id=%d T=%d A=%d M=%d I=%d\n", prototype_name, prototype_id, (int)t, (int)a, (int)m, (int)i);
        }
        // Map hash->name for command-based spawns
        RegisterPrototypeHash(dmHashString64(prototype_name), prototype_name);
        
        lua_pop(L, 1);  // Remove template value, keep key for next iteration
    }
    
    return 0;
}

// === OBSERVER SYSTEM ===

static int L_set_observer(lua_State* L) {
    int32_t z = (int32_t)luaL_checkinteger(L, 1);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 3);
    int32_t hot_chunks = (int32_t)luaL_checkinteger(L, 4);  // Changed from hot_radius
    int32_t warm_chunks = (int32_t)luaL_checkinteger(L, 5); // Changed from warm_radius
    int32_t hot_z = (int32_t)luaL_checkinteger(L, 6);
    int32_t warm_z = (int32_t)luaL_checkinteger(L, 7);
    
    lua_pushinteger(L, SetObserver(z, tile_x, tile_y, hot_chunks, warm_chunks, hot_z, warm_z));
    return 1;
}

static int L_move_observer(lua_State* L) {
    int32_t id = (int32_t)luaL_checkinteger(L, 1);
    int32_t z = (int32_t)luaL_checkinteger(L, 2);
    int32_t tile_x = (int32_t)luaL_checkinteger(L, 3);
    int32_t tile_y = (int32_t)luaL_checkinteger(L, 4);
    
    MoveObserver(id, z, tile_x, tile_y);
    return 0;
}

// === OBSERVER SYSTEM FUNCTIONS ===

static int L_get_observers(lua_State* L) {
    const std::vector<Observer>& observers = GetObservers();
    
    lua_newtable(L);
    for(size_t i = 0; i < observers.size(); i++) {
        lua_createtable(L, 0, 7);
        
        lua_pushinteger(L, observers[i].id);
        lua_setfield(L, -2, "id");
        
        lua_pushinteger(L, observers[i].z);
        lua_setfield(L, -2, "z");
        
        lua_pushinteger(L, observers[i].tile_x);
        lua_setfield(L, -2, "tile_x");
        
        lua_pushinteger(L, observers[i].tile_y);
        lua_setfield(L, -2, "tile_y");
        
        lua_pushinteger(L, observers[i].hot_chunk_radius);  // Changed from hot_radius
        lua_setfield(L, -2, "hot_chunk_radius");
        
        lua_pushinteger(L, observers[i].warm_chunk_radius); // Changed from warm_radius
        lua_setfield(L, -2, "warm_chunk_radius");
        
        lua_pushinteger(L, observers[i].hot_z_layers);
        lua_setfield(L, -2, "hot_z_layers");
        
        lua_pushinteger(L, observers[i].warm_z_layers);
        lua_setfield(L, -2, "warm_z_layers");
        
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_get_hot_chunks(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    
    Floor* floor = GetFloorByZ(floor_z);
    if (!floor) {
        lua_newtable(L);  // Return empty table if floor doesn't exist
        return 1;
    }
    
    lua_newtable(L);
    int index = 1;
    
    for(const auto& chunk_key : floor->hot_chunks) {
        lua_createtable(L, 0, 3);
        
        // Extract chunk coordinates from packed key
        int16_t cx = (int16_t)(chunk_key >> 32);
        int16_t cy = (int16_t)(chunk_key & 0xffffffff);
        
        lua_pushinteger(L, floor_z);
        lua_setfield(L, -2, "floor_z");
        
        lua_pushinteger(L, cx);
        lua_setfield(L, -2, "chunk_x");
        
        lua_pushinteger(L, cy);
        lua_setfield(L, -2, "chunk_y");
        
        lua_rawseti(L, -2, index++);
    }
    
    return 1;
}

static int L_get_warm_chunks(lua_State* L) {
    int32_t floor_z = (int32_t)luaL_checkinteger(L, 1);
    
    Floor* floor = GetFloorByZ(floor_z);
    if (!floor) {
        lua_newtable(L);  // Return empty table if floor doesn't exist
        return 1;
    }
    
    lua_newtable(L);
    int index = 1;
    
    for(const auto& chunk_key : floor->warm_chunks) {
        lua_createtable(L, 0, 3);
        
        // Extract chunk coordinates from packed key
        int16_t cx = (int16_t)(chunk_key >> 32);
        int16_t cy = (int16_t)(chunk_key & 0xffffffff);
        
        lua_pushinteger(L, floor_z);
        lua_setfield(L, -2, "floor_z");
        
        lua_pushinteger(L, cx);
        lua_setfield(L, -2, "chunk_x");
        
        lua_pushinteger(L, cy);
        lua_setfield(L, -2, "chunk_y");
        
        lua_rawseti(L, -2, index++);
    }
    
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

// === INVENTORY FUNCTIONS ===

static int L_inventory_add_to_slot(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t slot_index = (int32_t)luaL_checkinteger(L, 2);
    ItemType item = (ItemType)luaL_checkinteger(L, 3);
    int32_t amount = (int32_t)luaL_checkinteger(L, 4);
    
    bool success = Inventory_AddToSlot(entity_id, slot_index, item, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_remove_from_slot(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t slot_index = (int32_t)luaL_checkinteger(L, 2);
    ItemType item = (ItemType)luaL_checkinteger(L, 3);
    int32_t amount = (int32_t)luaL_checkinteger(L, 4);
    
    bool success = Inventory_RemoveFromSlot(entity_id, slot_index, item, amount);
    lua_pushboolean(L, success);
    return 1;
}

static int L_inventory_get_slot_item(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t slot_index = (int32_t)luaL_checkinteger(L, 2);
    
    ItemType item = Inventory_GetSlotItem(entity_id, slot_index);
    lua_pushinteger(L, item);
    return 1;
}

static int L_inventory_get_slot_quantity(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t slot_index = (int32_t)luaL_checkinteger(L, 2);
    
    int32_t quantity = Inventory_GetSlotQuantity(entity_id, slot_index);
    lua_pushinteger(L, quantity);
    return 1;
}

static int L_inventory_get_output_slots(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    std::vector<int32_t> output_slots = Inventory_GetOutputSlots(entity_id);
    
    lua_newtable(L);
    for (size_t i = 0; i < output_slots.size(); i++) {
        lua_pushinteger(L, output_slots[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

static int L_inventory_get_slot_count(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory) {
        lua_pushinteger(L, 0);
        return 1;
    }
    
    lua_pushinteger(L, (int32_t)inventory->slots.size());
    return 1;
}

static int L_inventory_is_slot_output(lua_State* L) {
    EntityId entity_id = (EntityId)luaL_checkinteger(L, 1);
    int32_t slot_index = (int32_t)luaL_checkinteger(L, 2);
    
    bool is_output = Inventory_IsSlotOutput(entity_id, slot_index);
    lua_pushboolean(L, is_output);
    return 1;
}

// Add this function to get entities by prototype name
static int L_get_entities_by_prototype(lua_State* L) {
    const char* prototype_name = luaL_checkstring(L, 1);
    
    std::vector<EntityId> result;
    const std::vector<Entity>& entities = GetAllEntities();
    
    for(const Entity& entity : entities) {
        if(entity.prototype_name == prototype_name) {
            result.push_back(entity.id);
        }
    }
    
    lua_newtable(L);
    for(size_t i = 0; i < result.size(); i++) {
        lua_pushinteger(L, result[i]);
        lua_rawseti(L, -2, i + 1);
    }
    
    return 1;
}

// === FUNCTION REGISTRATION ===

static const luaL_Reg sim_functions[] = {
    // World functions
    {"spawn_floor_at_z", L_spawn_floor_at_z},
    {"get_active_chunks", L_get_active_chunks},
    
    // Entity functions
    {"get_entity", L_get_entity},
    {"destroy_entity", L_destroy_entity},
    {"move_entity", L_move_entity},
    {"set_entity_position", L_set_entity_position},
    {"set_entity_floor", L_set_entity_floor},
    
    // Component access functions
    {"get_entity_metadata", L_get_entity_metadata},
    {"get_entity_visual_config", L_get_entity_visual_config},
    {"get_entity_transform", L_get_entity_transform},
    {"get_entity_animation_state", L_get_entity_animation_state},
    
    // Spatial query functions
    {"get_entities_in_chunk", L_get_entities_in_chunk},
    {"get_entities_in_radius", L_get_entities_in_radius},
    {"get_entities_at_tile", L_get_entities_at_tile},
    {"get_entities_on_floor", L_get_entities_on_floor},
    {"get_entities_by_prototype", L_get_entities_by_prototype},
    
    // Prototype registration
    {"register_entity_prototypes", L_register_entity_prototypes},
    
    // Observer system
    {"set_observer", L_set_observer},
    {"move_observer", L_move_observer},
    {"get_observers", L_get_observers},           // ← NEW
    {"get_hot_chunks", L_get_hot_chunks},         // ← NEW
    {"get_warm_chunks", L_get_warm_chunks},       // ← NEW
    
    // Floor management
    {"set_current_floor", L_set_current_floor},
    {"get_current_floor", L_get_current_floor},
    
    // Inventory functions
    {"inventory_add_to_slot", L_inventory_add_to_slot},
    {"inventory_remove_from_slot", L_inventory_remove_from_slot},
    {"inventory_get_slot_item", L_inventory_get_slot_item},
    {"inventory_get_slot_quantity", L_inventory_get_slot_quantity},
    {"inventory_get_slot_count", L_inventory_get_slot_count},
    {"inventory_is_slot_output", L_inventory_is_slot_output},
    {"inventory_get_output_slots", L_inventory_get_output_slots},
    
    {NULL, NULL}
};

} // namespace simcore

// Lua registration function (called from sim_entry.cpp)
void simcore::LuaRegister_World(lua_State* L) {
    luaL_register(L, "sim", simcore::sim_functions);
}