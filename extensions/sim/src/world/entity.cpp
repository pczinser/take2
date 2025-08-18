#include "entity.hpp"
#include "world.hpp"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>

namespace simcore {

static std::vector<Entity> g_entities;
static int32_t g_next_entity_id = 1;
static std::unordered_map<std::string, EntityTemplate> g_entity_templates;

// Map chunk coordinates to entity IDs
static std::unordered_map<int64_t, std::vector<int32_t>> g_chunk_entities;

// Current floor for floor-based queries
static int32_t g_current_floor_z = 0;

// Helper to pack chunk coordinates
static inline int64_t PackChunkCoords(int32_t z, int32_t cx, int32_t cy) {
    return ((int64_t)z << 32) | ((int64_t)cx << 16) | (int64_t)cy;
}

EntityType GetEntityTypeFromString(const std::string& type_str) {
    if(type_str == "player") return ENTITY_PLAYER;
    if(type_str == "building") return ENTITY_BUILDING;
    // ... other types will be added later
    return ENTITY_PLAYER; // default
}

int32_t CreateEntity(const std::string& template_name, float grid_x, float grid_y, int32_t floor_z) {
    auto it = g_entity_templates.find(template_name);
    if(it == g_entity_templates.end()) {
        printf("Unknown entity template: %s\n", template_name.c_str());
        return -1;
    }
    
    const EntityTemplate& templ = it->second;
    
    // Check if building can be placed (for buildings only)
    if(templ.type == "building") {
        if(!CanPlaceBuilding(floor_z, (int32_t)grid_x, (int32_t)grid_y, templ.width, templ.height)) {
            printf("ERROR: Cannot place %s at grid (%.1f, %.1f) on floor %d\n", 
                   template_name.c_str(), grid_x, grid_y, floor_z);
            return -1;
        }
    }
    
    // Check if the floor exists, create it if it doesn't
    Floor* floor = GetFloorByZ(floor_z);
    if(!floor) {
        int32_t max_chunks = GetFloorMaxChunks(floor_z);
        int32_t chunks_w = (floor_z > 0) ? 2 : 4;
        int32_t chunks_h = (floor_z > 0) ? 2 : 4;
        SpawnFloorAtZ(floor_z, chunks_w, chunks_h, 32, 32);
        printf("Auto-created floor %d with max %d chunks\n", floor_z, max_chunks);
    }
    
    Entity entity;
    entity.id = g_next_entity_id++;
    entity.type = GetEntityTypeFromString(templ.type);
    entity.grid_x = grid_x;
    entity.grid_y = grid_y;
    entity.chunk_x = (int32_t)(grid_x / 32.0f);
    entity.chunk_y = (int32_t)(grid_y / 32.0f);
    entity.floor_z = floor_z;
    entity.width = templ.width;
    entity.height = templ.height;
    entity.properties = templ.properties;
    entity.int_properties = templ.int_properties;
    entity.is_dirty = false;
    
    g_entities.push_back(entity);
    
    // Add entity to ALL occupied chunks
    auto occupied_chunks = GetBuildingOccupiedChunks((int32_t)grid_x, (int32_t)grid_y, templ.width, templ.height);
    for(const auto& [chunk_x, chunk_y] : occupied_chunks) {
        int64_t chunk_key = PackChunkCoords(floor_z, chunk_x, chunk_y);
        g_chunk_entities[chunk_key].push_back(entity.id);
    }
    
    printf("Created %s %d at grid (%.1f, %.1f) size %dx%d on floor %d\n", 
           template_name.c_str(), entity.id, grid_x, grid_y, templ.width, templ.height, floor_z);
    
    return entity.id;
}

Entity* GetEntity(int32_t id) {
    for(auto& entity : g_entities) {
        if(entity.id == id) return &entity;
    }
    return nullptr;
}

void UpdateEntityChunk(Entity* entity, int32_t old_chunk_x, int32_t old_chunk_y) {
    // Remove from old chunk
    int64_t old_chunk_key = PackChunkCoords(entity->floor_z, old_chunk_x, old_chunk_y);
    auto& old_chunk_entities = g_chunk_entities[old_chunk_key];
    old_chunk_entities.erase(std::remove(old_chunk_entities.begin(), old_chunk_entities.end(), entity->id), old_chunk_entities.end());
    
    // Add to new chunk
    int64_t new_chunk_key = PackChunkCoords(entity->floor_z, entity->chunk_x, entity->chunk_y);
    g_chunk_entities[new_chunk_key].push_back(entity->id);
    
    printf("Entity %d moved from chunk (%d, %d) to (%d, %d)\n", 
           entity->id, old_chunk_x, old_chunk_y, entity->chunk_x, entity->chunk_y);
}

void MoveEntity(int32_t id, float dx, float dy) {
    Entity* entity = GetEntity(id);
    if(!entity) return;
    
    float new_grid_x = entity->grid_x + dx;
    float new_grid_y = entity->grid_y + dy;
    int32_t new_chunk_x = (int32_t)(new_grid_x / 32.0f);
    int32_t new_chunk_y = (int32_t)(new_grid_y / 32.0f);
    
    // Check if new position is within chunk limits
    if(!CanCreateChunkOnFloor(entity->floor_z, new_chunk_x, new_chunk_y)) {
        printf("Entity %d movement blocked - would exceed floor %d chunk limits\n", 
               entity->id, entity->floor_z);
        return; // Block movement
    }
    
    int32_t old_chunk_x = entity->chunk_x;
    int32_t old_chunk_y = entity->chunk_y;
    
    entity->grid_x += dx;
    entity->grid_y += dy;
    entity->chunk_x = (int32_t)(entity->grid_x / 32.0f);
    entity->chunk_y = (int32_t)(entity->grid_y / 32.0f);
    
    if(old_chunk_x != entity->chunk_x || old_chunk_y != entity->chunk_y) {
        UpdateEntityChunk(entity, old_chunk_x, old_chunk_y);
    }
}

void SetEntityPosition(int32_t id, float grid_x, float grid_y) {
    Entity* entity = GetEntity(id);
    if(!entity) return;
    
    int32_t old_chunk_x = entity->chunk_x;
    int32_t old_chunk_y = entity->chunk_y;
    
    entity->grid_x = grid_x;
    entity->grid_y = grid_y;
    entity->chunk_x = (int32_t)(grid_x / 32.0f);
    entity->chunk_y = (int32_t)(grid_y / 32.0f);
    
    if(old_chunk_x != entity->chunk_x || old_chunk_y != entity->chunk_y) {
        UpdateEntityChunk(entity, old_chunk_x, old_chunk_y);
    }
}

bool DoBuildingsOverlap(int32_t x1, int32_t y1, int32_t w1, int32_t h1,
                       int32_t x2, int32_t y2, int32_t w2, int32_t h2) {
    return !(x1 + w1 <= x2 || x2 + w2 <= x1 || y1 + h1 <= y2 || y2 + h2 <= y1);
}

std::vector<int32_t> GetEntitiesInChunk(int32_t z, int32_t chunk_x, int32_t chunk_y) {
    int64_t chunk_key = PackChunkCoords(z, chunk_x, chunk_y);
    auto it = g_chunk_entities.find(chunk_key);
    if(it != g_chunk_entities.end()) {
        return it->second;
    }
    return std::vector<int32_t>();
}

std::vector<int32_t> GetEntitiesInRadius(float grid_x, float grid_y, float radius) {
    std::vector<int32_t> result;
    for(const auto& entity : g_entities) {
        float dx = entity.grid_x - grid_x;
        float dy = entity.grid_y - grid_y;
        float distance = sqrt(dx*dx + dy*dy);
        if(distance <= radius) {
            result.push_back(entity.id);
        }
    }
    return result;
}

const std::vector<Entity>& GetAllEntities() {
    return g_entities;
}

void RegisterEntityTemplate(const std::string& name, const EntityTemplate& templ) {
    g_entity_templates[name] = templ;
}

EntityTemplate* GetEntityTemplate(const std::string& name) {
    auto it = g_entity_templates.find(name);
    return it != g_entity_templates.end() ? &it->second : nullptr;
}

// Floor management functions
void SetCurrentFloor(int32_t floor_z) {
    g_current_floor_z = floor_z;
    printf("Switched to floor %d\n", floor_z);
}

int32_t GetCurrentFloor() {
    return g_current_floor_z;
}

std::vector<int32_t> GetEntitiesOnFloor(int32_t floor_z) {
    std::vector<int32_t> result;
    for(const auto& entity : g_entities) {
        if(entity.floor_z == floor_z) {
            result.push_back(entity.id);
        }
    }
    return result;
}

void SetEntityFloor(int32_t id, int32_t floor_z) {
    Entity* entity = GetEntity(id);
    if(!entity) return;
    
    int32_t old_floor_z = entity->floor_z;
    entity->floor_z = floor_z;
    
    // Check if the new floor exists, create it if it doesn't
    Floor* floor = GetFloorByZ(floor_z);
    if(!floor) {
        SpawnFloorAtZ(floor_z, 2, 2, 32, 32);
        printf("Auto-created floor %d\n", floor_z);
    }
    
    // Update chunk tracking for the new floor
    int64_t old_chunk_key = PackChunkCoords(old_floor_z, entity->chunk_x, entity->chunk_y);
    int64_t new_chunk_key = PackChunkCoords(floor_z, entity->chunk_x, entity->chunk_y);
    
    // Remove from old chunk
    auto& old_chunk_entities = g_chunk_entities[old_chunk_key];
    old_chunk_entities.erase(std::remove(old_chunk_entities.begin(), old_chunk_entities.end(), entity->id), old_chunk_entities.end());
    
    // Add to new chunk
    g_chunk_entities[new_chunk_key].push_back(entity->id);
    
    printf("Entity %d moved from floor %d to floor %d\n", entity->id, old_floor_z, floor_z);
}

std::vector<std::pair<int32_t, int32_t>> GetBuildingOccupiedChunks(int32_t base_x, int32_t base_y, int32_t width, int32_t height) {
    std::vector<std::pair<int32_t, int32_t>> chunks;
    
    // Calculate which chunks the building occupies
    int32_t start_chunk_x = base_x / 32;
    int32_t start_chunk_y = base_y / 32;
    int32_t end_chunk_x = (base_x + width - 1) / 32;
    int32_t end_chunk_y = (base_y + height - 1) / 32;
    
    for(int32_t cx = start_chunk_x; cx <= end_chunk_x; ++cx) {
        for(int32_t cy = start_chunk_y; cy <= end_chunk_y; ++cy) {
            chunks.push_back({cx, cy});
        }
    }
    
    return chunks;
}

bool CanPlaceBuilding(int32_t floor_z, int32_t base_x, int32_t base_y, int32_t width, int32_t height) {
    // Check if building fits within floor chunk limits
    auto occupied_chunks = GetBuildingOccupiedChunks(base_x, base_y, width, height);
    
    for(const auto& [chunk_x, chunk_y] : occupied_chunks) {
        if(!CanCreateChunkOnFloor(floor_z, chunk_x, chunk_y)) {
            return false; // Building would exceed floor limits
        }
    }
    
    // Check if any of the occupied chunks already have buildings that overlap
    for(const auto& [chunk_x, chunk_y] : occupied_chunks) {
        auto entities_in_chunk = GetEntitiesInChunk(floor_z, chunk_x, chunk_y);
        for(int32_t entity_id : entities_in_chunk) {
            Entity* existing_entity = GetEntity(entity_id);
            if(existing_entity && existing_entity->type == ENTITY_BUILDING) {
                // Check if buildings overlap
                if(DoBuildingsOverlap(base_x, base_y, width, height,
                                     (int32_t)existing_entity->grid_x, (int32_t)existing_entity->grid_y,
                                     existing_entity->width, existing_entity->height)) {
                    return false; // Buildings overlap
                }
            }
        }
    }
    
    return true;
}

std::vector<std::pair<int32_t, int32_t>> GetEntityOccupiedTiles(int32_t entity_id) {
    std::vector<std::pair<int32_t, int32_t>> tiles;
    
    Entity* entity = GetEntity(entity_id);
    if(!entity) return tiles;
    
    int32_t start_x = (int32_t)entity->grid_x;
    int32_t start_y = (int32_t)entity->grid_y;
    int32_t end_x = start_x + entity->width - 1;
    int32_t end_y = start_y + entity->height - 1;
    
    for(int32_t y = start_y; y <= end_y; ++y) {
        for(int32_t x = start_x; x <= end_x; ++x) {
            tiles.push_back({x, y});
        }
    }
    
    return tiles;
}

std::vector<std::pair<int32_t, int32_t>> GetEntityOccupiedChunks(int32_t entity_id) {
    std::vector<std::pair<int32_t, int32_t>> chunks;
    
    Entity* entity = GetEntity(entity_id);
    if(!entity) return chunks;
    
    return GetBuildingOccupiedChunks((int32_t)entity->grid_x, (int32_t)entity->grid_y, entity->width, entity->height);
}

std::vector<int32_t> GetEntitiesAtTile(int32_t floor_z, int32_t tile_x, int32_t tile_y) {
    std::vector<int32_t> entities;
    
    // Get the chunk this tile belongs to
    int32_t chunk_x = tile_x / 32;
    int32_t chunk_y = tile_y / 32;
    
    // Get all entities in this chunk
    auto chunk_entities = GetEntitiesInChunk(floor_z, chunk_x, chunk_y);
    
    // Check which entities actually occupy this specific tile
    for(int32_t entity_id : chunk_entities) {
        Entity* entity = GetEntity(entity_id);
        if(entity && entity->floor_z == floor_z) {
            int32_t entity_start_x = (int32_t)entity->grid_x;
            int32_t entity_start_y = (int32_t)entity->grid_y;
            int32_t entity_end_x = entity_start_x + entity->width - 1;
            int32_t entity_end_y = entity_start_y + entity->height - 1;
            
            // Check if tile is within entity bounds
            if(tile_x >= entity_start_x && tile_x <= entity_end_x &&
               tile_y >= entity_start_y && tile_y <= entity_end_y) {
                entities.push_back(entity_id);
            }
        }
    }
    
    return entities;
}

void RegisterDefaultEntityTemplates() {
    // Register basic building template
    EntityTemplate building_template;
    building_template.type = "building";
    building_template.width = 1;
    building_template.height = 1;
    building_template.properties["production_rate"] = 10.0;
    building_template.int_properties["health"] = 100;
    
    RegisterEntityTemplate("building", building_template);
    printf("Registered basic building template\n");
}

} // namespace simcore
