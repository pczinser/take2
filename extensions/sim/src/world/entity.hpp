#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "../components/component_registry.hpp"

namespace simcore {

// Forward declaration
struct Tile;

struct EntityTemplate {
    const std::string type;
    const int32_t width;
    const int32_t height;
    const std::unordered_map<std::string, float> properties;
    const std::unordered_map<std::string, int32_t> int_properties;
    
    // Constructor to initialize const members
    EntityTemplate(const std::string& t, int32_t w, int32_t h, 
                   const std::unordered_map<std::string, float>& props,
                   const std::unordered_map<std::string, int32_t>& int_props)
        : type(t), width(w), height(h), properties(props), int_properties(int_props) {}
};

// Add inventory support to Entity struct
struct Entity {
    int32_t id;
    std::string category;  // "building", "player", "transport"
    std::string type;      // "extractor", "assembler", "storage"
    std::string name;      // "stone_extractor_1", "player_main"
    float grid_x, grid_y;  // Bottom-left corner (Defold coordinates)
    int32_t chunk_x, chunk_y;  // Bottom-left chunk
    int32_t floor_z;
    int32_t width = 1, height = 1;  // Building size in grid cells
    std::unordered_map<std::string, float> properties;
    std::unordered_map<std::string, int32_t> int_properties;
    std::vector<int32_t> inventory_ids;  // IDs of inventories owned by this entity
    bool is_dirty = false;
};

// Entity management functions
// Revert to the working template-based signature
int32_t CreateEntity(const std::string& template_name, float grid_x, float grid_y, int32_t floor_z);
Entity* GetEntity(int32_t id);
void MoveEntity(int32_t id, float dx, float dy);
void SetEntityPosition(int32_t id, float grid_x, float grid_y);
void SetEntityFloor(int32_t id, int32_t floor_z);
const std::vector<Entity>& GetAllEntities();

// Template management
void RegisterEntityTemplate(const std::string& name, const EntityTemplate& templ);
EntityTemplate* GetEntityTemplate(const std::string& name);
void RegisterDefaultEntityTemplates();

// Chunk-based entity management
std::vector<int32_t> GetEntitiesInChunk(int32_t z, int32_t chunk_x, int32_t chunk_y);
std::vector<int32_t> GetEntitiesInRadius(float grid_x, float grid_y, float radius);
std::vector<int32_t> GetEntitiesOnFloor(int32_t floor_z);

// Building placement functions
bool CanPlaceBuilding(int32_t floor_z, int32_t base_x, int32_t base_y, int32_t width, int32_t height);
std::vector<std::pair<int32_t, int32_t>> GetBuildingOccupiedChunks(int32_t base_x, int32_t base_y, int32_t width, int32_t height);
std::vector<std::pair<int32_t, int32_t>> GetEntityOccupiedTiles(int32_t entity_id);
std::vector<std::pair<int32_t, int32_t>> GetEntityOccupiedChunks(int32_t entity_id);
std::vector<int32_t> GetEntitiesAtTile(int32_t floor_z, int32_t tile_x, int32_t tile_y);

// Floor management
void SetCurrentFloor(int32_t floor_z);
int32_t GetCurrentFloor();

void UpdateEntityChunk(Entity* entity, int32_t old_chunk_x, int32_t old_chunk_y);

// Component creation from templates (Phase 2 migration support)
void CreateComponentsFromTemplate(int32_t entity_id, const std::string& template_name, float grid_x, float grid_y, int32_t floor_z);
void CreateComponentsFromEntity(int32_t entity_id, const Entity& entity);

} // namespace simcore
