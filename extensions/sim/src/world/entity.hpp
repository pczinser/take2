#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace simcore {

// Forward declaration
struct Tile;

enum EntityType {
    ENTITY_PLAYER,
    ENTITY_BUILDING,
    ENTITY_BELT,
    ENTITY_FACTORY,
    // ... other types will be added later
};

struct EntityTemplate {
    std::string type;
    int32_t width = 1, height = 1;
    std::unordered_map<std::string, float> properties;
    std::unordered_map<std::string, int32_t> int_properties;
};

struct Entity {
    int32_t id;
    EntityType type;
    float grid_x, grid_y;
    int32_t chunk_x, chunk_y;
    int32_t floor_z;
    int32_t width = 1, height = 1;
    std::unordered_map<std::string, float> properties;
    std::unordered_map<std::string, int32_t> int_properties;
    bool is_dirty = false;
};

// Entity management functions
int32_t CreateEntity(const std::string& template_name, float grid_x, float grid_y, int32_t floor_z = 0);
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

} // namespace simcore
