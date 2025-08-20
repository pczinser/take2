#include "entity.hpp"
#include "world.hpp"
#include "../systems/inventory_system.hpp"  // ← Add this to get ItemType
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>

namespace simcore {

// Entity storage
static std::vector<Entity> g_entities;
static EntityId g_next_entity_id = 1;
static std::unordered_map<std::string, EntityPrototype> g_entity_prototypes;

// Efficient chunk-to-entity mapping for spatial queries
static std::unordered_map<int64_t, std::vector<EntityId>> g_chunk_entities;

// Current floor for queries
static int32_t g_current_floor_z = 0;

// Helper to pack chunk coordinates for efficient lookup
static inline int64_t PackChunkCoords(int32_t z, int32_t cx, int32_t cy) {
    return ((int64_t)z << 32) | ((int64_t)cx << 16) | (int64_t)cy;
}

// === CHUNK MAPPING HELPERS ===

void AddEntityToChunkMapping(EntityId entity_id) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    components::BuildingComponent* building = components::g_building_components.GetComponent(entity_id);
    
    if (!transform) return;
    
    // Calculate which chunks this entity occupies
    int32_t start_chunk_x = transform->chunk_x;
    int32_t start_chunk_y = transform->chunk_y;
    int32_t end_chunk_x = start_chunk_x;
    int32_t end_chunk_y = start_chunk_y;
    
    // If entity has building component, it might span multiple chunks
    if (building) {
        int32_t entity_end_x = (int32_t)transform->grid_x + building->width - 1;
        int32_t entity_end_y = (int32_t)transform->grid_y + building->height - 1;
        end_chunk_x = entity_end_x / 32;
        end_chunk_y = entity_end_y / 32;
    }
    
    // Add entity to all chunks it occupies
    for (int32_t cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
        for (int32_t cy = start_chunk_y; cy <= end_chunk_y; ++cy) {
            int64_t chunk_key = PackChunkCoords(transform->floor_z, cx, cy);
            g_chunk_entities[chunk_key].push_back(entity_id);
        }
    }
    
    printf("Added entity %d to chunk mapping (chunks %d,%d to %d,%d on floor %d)\n", 
           entity_id, start_chunk_x, start_chunk_y, end_chunk_x, end_chunk_y, transform->floor_z);
}

void RemoveEntityFromChunkMapping(EntityId entity_id) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    components::BuildingComponent* building = components::g_building_components.GetComponent(entity_id);
    
    if (!transform) return;
    
    // Calculate which chunks this entity occupies
    int32_t start_chunk_x = transform->chunk_x;
    int32_t start_chunk_y = transform->chunk_y;
    int32_t end_chunk_x = start_chunk_x;
    int32_t end_chunk_y = start_chunk_y;
    
    if (building) {
        int32_t entity_end_x = (int32_t)transform->grid_x + building->width - 1;
        int32_t entity_end_y = (int32_t)transform->grid_y + building->height - 1;
        end_chunk_x = entity_end_x / 32;
        end_chunk_y = entity_end_y / 32;
    }
    
    // Remove entity from all chunks it occupied
    for (int32_t cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
        for (int32_t cy = start_chunk_y; cy <= end_chunk_y; ++cy) {
            int64_t chunk_key = PackChunkCoords(transform->floor_z, cx, cy);
            auto& chunk_entities = g_chunk_entities[chunk_key];
            chunk_entities.erase(
                std::remove(chunk_entities.begin(), chunk_entities.end(), entity_id),
                chunk_entities.end());
        }
    }
}

void UpdateEntityChunkMapping(EntityId entity_id, int32_t old_chunk_x, int32_t old_chunk_y, int32_t old_floor_z) {
    // Remove from old chunks
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    components::BuildingComponent* building = components::g_building_components.GetComponent(entity_id);
    
    if (!transform) return;
    
    // Remove from old position
    int32_t old_end_chunk_x = old_chunk_x;
    int32_t old_end_chunk_y = old_chunk_y;
    
    if (building) {
        // Calculate old occupied chunks (approximate)
        old_end_chunk_x = (old_chunk_x * 32 + building->width - 1) / 32;
        old_end_chunk_y = (old_chunk_y * 32 + building->height - 1) / 32;
    }
    
    for (int32_t cx = old_chunk_x; cx <= old_end_chunk_x; ++cx) {
        for (int32_t cy = old_chunk_y; cy <= old_end_chunk_y; ++cy) {
            int64_t old_chunk_key = PackChunkCoords(old_floor_z, cx, cy);
            auto& chunk_entities = g_chunk_entities[old_chunk_key];
            chunk_entities.erase(
                std::remove(chunk_entities.begin(), chunk_entities.end(), entity_id),
                chunk_entities.end());
        }
    }
    
    // Add to new position
    AddEntityToChunkMapping(entity_id);
}

// === ENTITY MANAGEMENT ===

EntityId CreateEntity(const std::string& prototype_name, float grid_x, float grid_y, int32_t floor_z) {
    auto it = g_entity_prototypes.find(prototype_name);
    if(it == g_entity_prototypes.end()) {
        printf("ERROR: Unknown entity prototype: %s\n", prototype_name.c_str());
        return -1;
    }
    
    // Clone the prototype entity
    return CloneEntity(it->second.prototype_id, grid_x, grid_y, floor_z);
}

EntityId CloneEntity(EntityId prototype_id, float grid_x, float grid_y, int32_t floor_z) {
    // Create new entity
    EntityId entity_id = g_next_entity_id++;
    Entity* prototype = GetEntity(prototype_id);
    if (!prototype) {
        printf("ERROR: Prototype entity %d not found\n", prototype_id);
        return -1;
    }
    
    Entity entity(entity_id, prototype->name, prototype->prototype_name);
    g_entities.push_back(entity);
    
    // Clone all components from prototype
    CloneComponentsFromEntity(prototype_id, entity_id, grid_x, grid_y, floor_z);
    
    // Add entity to chunk mapping for efficient spatial queries
    AddEntityToChunkMapping(entity_id);
    
    printf("Cloned entity %d from prototype %d at grid (%.1f, %.1f) on floor %d\n", 
           entity_id, prototype_id, grid_x, grid_y, floor_z);
    
    return entity_id;
}

Entity* GetEntity(EntityId id) {
    for(auto& entity : g_entities) {
        if(entity.id == id) return &entity;
    }
    return nullptr;
}

void DestroyEntity(EntityId id) {
    // Remove from chunk mapping
    RemoveEntityFromChunkMapping(id);
    
    // Remove all components
    components::RemoveAllComponents(id);
    
    // Remove entity
    g_entities.erase(
        std::remove_if(g_entities.begin(), g_entities.end(),
                      [id](const Entity& e) { return e.id == id; }),
        g_entities.end());
    
    printf("Destroyed entity %d and all its components\n", id);
}

const std::vector<Entity>& GetAllEntities() {
    return g_entities;
}

// === ENTITY MOVEMENT FUNCTIONS ===

void MoveEntity(EntityId id, float dx, float dy) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(id);
    if (!transform) return;
    
    float new_x = transform->grid_x + dx;
    float new_y = transform->grid_y + dy;
    int32_t new_chunk_x = (int32_t)(new_x / 32.0f);
    int32_t new_chunk_y = (int32_t)(new_y / 32.0f);
    
    // Check if new position is valid
    if (!CanCreateChunkOnFloor(transform->floor_z, new_chunk_x, new_chunk_y)) {
        printf("Entity %d movement blocked - would exceed floor limits\n", id);
        return;
    }
    
    // Store old position
    int32_t old_chunk_x = transform->chunk_x;
    int32_t old_chunk_y = transform->chunk_y;
    int32_t old_floor_z = transform->floor_z;
    
    // Update position
    transform->grid_x = new_x;
    transform->grid_y = new_y;
    transform->chunk_x = new_chunk_x;
    transform->chunk_y = new_chunk_y;
    
    // Mark dirty
    Entity* entity = GetEntity(id);
    if (entity) entity->is_dirty = true;
    
    // Update chunk mapping if needed
    if (old_chunk_x != new_chunk_x || old_chunk_y != new_chunk_y) {
        UpdateEntityChunkMapping(id, old_chunk_x, old_chunk_y, old_floor_z);
    }
}

void SetEntityPosition(EntityId id, float grid_x, float grid_y) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(id);
    if (!transform) return;
    
    // Store old position
    int32_t old_chunk_x = transform->chunk_x;
    int32_t old_chunk_y = transform->chunk_y;
    int32_t old_floor_z = transform->floor_z;
    
    // Update position
    transform->grid_x = grid_x;
    transform->grid_y = grid_y;
    transform->chunk_x = (int32_t)(grid_x / 32.0f);
    transform->chunk_y = (int32_t)(grid_y / 32.0f);
    
    // Mark dirty
    Entity* entity = GetEntity(id);
    if (entity) entity->is_dirty = true;
    
    // Update chunk mapping if needed
    if (old_chunk_x != transform->chunk_x || old_chunk_y != transform->chunk_y) {
        UpdateEntityChunkMapping(id, old_chunk_x, old_chunk_y, old_floor_z);
    }
}

void SetEntityFloor(EntityId id, int32_t floor_z) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(id);
    if (!transform) return;
    
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
    
    // Mark dirty
    Entity* entity = GetEntity(id);
    if (entity) entity->is_dirty = true;
    
    // Update chunk mapping (floor changed)
    UpdateEntityChunkMapping(id, transform->chunk_x, transform->chunk_y, old_floor_z);
    
    printf("Entity %d moved from floor %d to floor %d\n", id, old_floor_z, floor_z);
}

// === PROTOTYPE MANAGEMENT ===

void RegisterEntityPrototype(const std::string& name, EntityId prototype_id) {
    g_entity_prototypes[name] = EntityPrototype(prototype_id, name);
    printf("Registered entity prototype: %s (ID: %d)\n", name.c_str(), prototype_id);
}

EntityPrototype* GetEntityPrototype(const std::string& name) {
    auto it = g_entity_prototypes.find(name);
    return it != g_entity_prototypes.end() ? &it->second : nullptr;
}

void ClearEntityPrototypes() {
    g_entity_prototypes.clear();
}

void RegisterDefaultEntityPrototypes() {
    // This will be handled by Lua registration now
    printf("Default entity prototypes will be registered by Lua\n");
}

// === COMPONENT CLONING ===

void CloneComponentsFromEntity(EntityId source_id, EntityId target_id, float grid_x, float grid_y, int32_t floor_z) {
    // Clone metadata component
    components::MetadataComponent* source_metadata = components::g_metadata_components.GetComponent(source_id);
    if (source_metadata) {
        components::g_metadata_components.AddComponent(target_id, *source_metadata);
    }
    
    // Clone transform component (with new position)
    components::TransformComponent* source_transform = components::g_transform_components.GetComponent(source_id);
    if (source_transform) {
        components::TransformComponent new_transform = *source_transform;
        new_transform.grid_x = grid_x;
        new_transform.grid_y = grid_y;
        new_transform.floor_z = floor_z;
        new_transform.chunk_x = (int32_t)(grid_x / 32.0f);
        new_transform.chunk_y = (int32_t)(grid_y / 32.0f);
        components::g_transform_components.AddComponent(target_id, new_transform);
    }
    
    // Clone building component
    components::BuildingComponent* source_building = components::g_building_components.GetComponent(source_id);
    if (source_building) {
        components::g_building_components.AddComponent(target_id, *source_building);
    }
    
    // Clone production component
    components::ProductionComponent* source_production = components::g_production_components.GetComponent(source_id);
    if (source_production) {
        components::g_production_components.AddComponent(target_id, *source_production);
    }
    
    // Clone health component
    components::HealthComponent* source_health = components::g_health_components.GetComponent(source_id);
    if (source_health) {
        components::g_health_components.AddComponent(target_id, *source_health);
    }
    
    // Clone inventory component (just copy the slot definitions)
    components::InventoryComponent* source_inventory = components::g_inventory_components.GetComponent(source_id);
    if (source_inventory) {
        components::g_inventory_components.AddComponent(target_id, *source_inventory);
    }
    
    printf("Cloned all components from entity %d to entity %d\n", source_id, target_id);
}

// === COMPONENT CREATION (for prototype creation) ===

// Remove this function entirely - it doesn't belong in the C++ core
// void CreateComponentInstancesFromLua(EntityId entity_id, const std::string& prototype_name, lua_State* L, int prototype_index, float grid_x, float grid_y, int32_t floor_z) {
//     // Push the template table to the top of the stack
//     lua_pushvalue(L, prototype_index);
    
//     // Check if components table exists
//     lua_getfield(L, -1, "components");
//     if (!lua_istable(L, -1)) {
//         printf("Warning: Template %s has no components table\n", prototype_name.c_str());
//         lua_pop(L, 2); // Pop components and template
//         return;
//     }
    
//     // === METADATA COMPONENT ===
//     lua_getfield(L, -1, "metadata");
//     if (lua_istable(L, -1)) {
//         std::string display_name = prototype_name;  // Default to template name
//         std::string category = "entity";           // Default category
        
//         lua_getfield(L, -1, "display_name");
//         if (lua_isstring(L, -1)) {
//             display_name = lua_tostring(L, -1);
//         }
//         lua_pop(L, 1);
        
//         lua_getfield(L, -1, "category");
//         if (lua_isstring(L, -1)) {
//             category = lua_tostring(L, -1);
//         }
//         lua_pop(L, 1);
        
//         components::g_metadata_components.AddComponent(entity_id, 
//             components::MetadataComponent(display_name, category));
//     }
//     lua_pop(L, 1); // Pop metadata table
    
//     // === TRANSFORM COMPONENT ===
//     lua_getfield(L, -1, "transform");
//     if (lua_istable(L, -1)) {
//         float move_speed = 100.0f;  // Default speed
        
//         lua_getfield(L, -1, "move_speed");
//         if (lua_isnumber(L, -1)) {
//             move_speed = (float)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         components::g_transform_components.AddComponent(entity_id, 
//             components::TransformComponent(grid_x, grid_y, floor_z, move_speed));
//     }
//     lua_pop(L, 1); // Pop transform table
    
//     // === BUILDING COMPONENT ===
//     lua_getfield(L, -1, "building");
//     if (lua_istable(L, -1)) {
//         int32_t width = 1;
//         int32_t height = 1;
//         std::string building_type = "generic";
        
//         lua_getfield(L, -1, "width");
//         if (lua_isnumber(L, -1)) {
//             width = (int32_t)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         lua_getfield(L, -1, "height");
//         if (lua_isnumber(L, -1)) {
//             height = (int32_t)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         lua_getfield(L, -1, "type");
//         if (lua_isstring(L, -1)) {
//             building_type = lua_tostring(L, -1);
//         }
//         lua_pop(L, 1);
        
//         components::g_building_components.AddComponent(entity_id, 
//             components::BuildingComponent(width, height, building_type));
//     }
//     lua_pop(L, 1); // Pop building table
    
//     // === PRODUCTION COMPONENT ===
//     lua_getfield(L, -1, "production");
//     if (lua_istable(L, -1)) {
//         components::ProductionComponent prod_comp;
        
//         lua_getfield(L, -1, "rate");
//         if (lua_isnumber(L, -1)) {
//             prod_comp.production_rate = (float)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         lua_getfield(L, -1, "extraction_rate");
//         if (lua_isnumber(L, -1)) {
//             prod_comp.extraction_rate = (float)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         lua_getfield(L, -1, "target_resource");
//         if (lua_isnumber(L, -1)) {
//             prod_comp.target_resource = (int32_t)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         components::g_production_components.AddComponent(entity_id, prod_comp);
//     }
//     lua_pop(L, 1); // Pop production table
    
//     // === HEALTH COMPONENT ===
//     lua_getfield(L, -1, "health");
//     if (lua_istable(L, -1)) {
//         int32_t health_amount = 100;
        
//         lua_getfield(L, -1, "amount");
//         if (lua_isnumber(L, -1)) {
//             health_amount = (int32_t)lua_tonumber(L, -1);
//         }
//         lua_pop(L, 1);
        
//         components::g_health_components.AddComponent(entity_id, 
//             components::HealthComponent(health_amount));
//     }
//     lua_pop(L, 1); // Pop health table
    
//     // === INVENTORY COMPONENT ===
//     lua_getfield(L, -1, "inventory");
//     if (lua_istable(L, -1)) {
//         std::vector<components::InventoryComponent::InventorySlot> slots;
        
//         lua_getfield(L, -1, "slots");
//         if (lua_istable(L, -1)) {
//             int slots_count = lua_objlen(L, -1);
//             for (int i = 1; i <= slots_count; i++) {
//                 lua_rawgeti(L, -1, i);
//                 if (lua_istable(L, -1)) {
//                     bool is_output = false;
//                     std::vector<ItemType> whitelist;
                    
//                     lua_getfield(L, -1, "is_output");
//                     if (lua_isboolean(L, -1)) {
//                         is_output = lua_toboolean(L, -1);
//                     }
//                     lua_pop(L, 1);
                    
//                     lua_getfield(L, -1, "whitelist");
//                     if (lua_istable(L, -1)) {
//                         int whitelist_count = lua_objlen(L, -1);
//                         for (int j = 1; j <= whitelist_count; j++) {
//                             lua_rawgeti(L, -1, j);
//                             if (lua_isnumber(L, -1)) {
//                                 whitelist.push_back((ItemType)lua_tonumber(L, -1));
//                             }
//                             lua_pop(L, 1);
//                         }
//                     }
//                     lua_pop(L, 1);
                    
//                     slots.push_back(components::InventoryComponent::InventorySlot(is_output, whitelist));
//                 }
//                 lua_pop(L, 1); // Pop slot table
//             }
//         }
//         lua_pop(L, 1); // Pop slots table
        
//         if (!slots.empty()) {
//             components::g_inventory_components.AddComponent(entity_id, 
//                 components::InventoryComponent(slots));
//         }
//     }
//     lua_pop(L, 1); // Pop inventory table
    
//     // Clean up stack
//     lua_pop(L, 2); // Pop components and template
    
//     printf("Created components for entity %d from template %s\n", entity_id, prototype_name.c_str());
// }

// === SPATIAL QUERIES (using components) ===

std::vector<EntityId> GetEntitiesInChunk(int32_t z, int32_t chunk_x, int32_t chunk_y) {
    // Efficient O(1) lookup using chunk mapping
    int64_t chunk_key = PackChunkCoords(z, chunk_x, chunk_y);
    auto it = g_chunk_entities.find(chunk_key);
    
    if (it != g_chunk_entities.end()) {
        return it->second;  // Return copy of entity list
    }
    
    return std::vector<EntityId>();  // Empty vector if no entities in chunk
}

std::vector<EntityId> GetEntitiesInRadius(float grid_x, float grid_y, float radius) {
    std::vector<EntityId> result;
    
    auto entities_with_transform = components::g_transform_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_transform) {
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        if(transform) {
            float dx = transform->grid_x - grid_x;
            float dy = transform->grid_y - grid_y;
            float distance = sqrt(dx*dx + dy*dy);
            if(distance <= radius) {
                result.push_back(entity_id);
            }
        }
    }
    
    return result;
}

std::vector<EntityId> GetEntitiesOnFloor(int32_t floor_z) {
    std::vector<EntityId> result;
    
    auto entities_with_transform = components::g_transform_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_transform) {
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        if(transform && transform->floor_z == floor_z) {
            result.push_back(entity_id);
        }
    }
    
    return result;
}

std::vector<EntityId> GetEntitiesAtTile(int32_t floor_z, int32_t tile_x, int32_t tile_y) {
    std::vector<EntityId> result;
    
    auto entities_with_transform = components::g_transform_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_transform) {
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        if(transform && transform->floor_z == floor_z) {
            // Check if entity is at this tile
            int32_t entity_tile_x = (int32_t)transform->grid_x;
            int32_t entity_tile_y = (int32_t)transform->grid_y;
            
            if(entity_tile_x == tile_x && entity_tile_y == tile_y) {
                result.push_back(entity_id);
            }
        }
    }
    
    return result;
}

// === BUILDING PLACEMENT ===

bool CanPlaceBuilding(int32_t floor_z, int32_t base_x, int32_t base_y, int32_t width, int32_t height) {
    // Check if the area is within floor limits
    int32_t end_x = base_x + width - 1;
    int32_t end_y = base_y + height - 1;
    
    int32_t start_chunk_x = base_x / 32;
    int32_t start_chunk_y = base_y / 32;
    int32_t end_chunk_x = end_x / 32;
    int32_t end_chunk_y = end_y / 32;
    
    // Check if all chunks are within floor limits
    for(int32_t cx = start_chunk_x; cx <= end_chunk_x; cx++) {
        for(int32_t cy = start_chunk_y; cy <= end_chunk_y; cy++) {
            if(!CanCreateChunkOnFloor(floor_z, cx, cy)) {
                return false;
            }
        }
    }
    
    return true;
}

// === SYSTEM INITIALIZATION ===

void InitializeEntitySystem() {
    g_entities.clear();
    g_next_entity_id = 1;
    g_entity_prototypes.clear();
    g_chunk_entities.clear();
    g_current_floor_z = 0;
    
    printf("Entity system initialized\n");
}

void ClearEntitySystem() {
    g_entities.clear();
    g_next_entity_id = 1;
    g_entity_prototypes.clear();
    g_chunk_entities.clear();
    
    printf("Entity system cleared\n");
}

// === FLOOR MANAGEMENT ===

void SetCurrentFloor(int32_t floor_z) {
    g_current_floor_z = floor_z;
}

int32_t GetCurrentFloor() {
    return g_current_floor_z;
}

EntityId GetNextEntityId() {
    return g_next_entity_id++;
}

void AddEntityToList(const Entity& entity) {
    g_entities.push_back(entity);
}

} // namespace simcore