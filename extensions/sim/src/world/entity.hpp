#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "../components/component_registry.hpp"

namespace simcore {

// Remove this line since EntityId is now in global simcore namespace
// using EntityId = components::EntityId;

// Simple EntityPrototype - just stores reference to prototype entity
struct EntityPrototype {
    EntityId prototype_id;  // The actual prototype entity ID
    std::string name;       // "miner", "player", etc.
    
    EntityPrototype() = default;
    EntityPrototype(EntityId id, const std::string& prototype_name) 
        : prototype_id(id), name(prototype_name) {}
};

// Minimal Entity - just core identity, everything else in components
struct Entity {
    EntityId id;
    std::string name;           // For debugging/identification  
    std::string prototype_name; // Which prototype this entity was cloned from
    bool is_dirty = false;     // For rendering system
    
    Entity() = default;
    Entity(EntityId entity_id, const std::string& entity_name, const std::string& proto_name)
        : id(entity_id), name(entity_name), prototype_name(proto_name) {}
};

// === ENTITY MANAGEMENT ===
EntityId CreateEntity(const std::string& prototype_name, float grid_x, float grid_y, int32_t floor_z);
EntityId CloneEntity(EntityId prototype_id, float grid_x, float grid_y, int32_t floor_z);
Entity* GetEntity(EntityId id);
void DestroyEntity(EntityId id);  // Removes entity and all its components
const std::vector<Entity>& GetAllEntities();

// Entity movement functions
void MoveEntity(EntityId id, float dx, float dy);
void SetEntityPosition(EntityId id, float grid_x, float grid_y);
void SetEntityFloor(EntityId id, int32_t floor_z);

// === PROTOTYPE MANAGEMENT ===
void RegisterEntityPrototype(const std::string& name, EntityId prototype_id);
EntityPrototype* GetEntityPrototype(const std::string& name);
void ClearEntityPrototypes();
void RegisterDefaultEntityPrototypes();

// === COMPONENT CREATION ===
// REMOVE THIS LINE - it doesn't belong in the C++ core:
// void CreateComponentInstancesFromLua(EntityId entity_id, const std::string& prototype_name, lua_State* L, int prototype_index, float grid_x, float grid_y, int32_t floor_z);

// Chunk mapping functions (for performance)
void AddEntityToChunkMapping(EntityId entity_id);
void RemoveEntityFromChunkMapping(EntityId entity_id);
void UpdateEntityChunkMapping(EntityId entity_id, int32_t old_chunk_x, int32_t old_chunk_y, int32_t old_floor_z);

// === SPATIAL QUERIES (using components) ===
std::vector<EntityId> GetEntitiesInChunk(int32_t z, int32_t chunk_x, int32_t chunk_y);
std::vector<EntityId> GetEntitiesInRadius(float grid_x, float grid_y, float radius);
std::vector<EntityId> GetEntitiesOnFloor(int32_t floor_z);
std::vector<EntityId> GetEntitiesAtTile(int32_t floor_z, int32_t tile_x, int32_t tile_y);

// === BUILDING PLACEMENT ===
bool CanPlaceBuilding(int32_t floor_z, int32_t base_x, int32_t base_y, int32_t width, int32_t height);

// === SYSTEM INITIALIZATION ===
void InitializeEntitySystem();
void ClearEntitySystem();

// === FLOOR MANAGEMENT ===
void SetCurrentFloor(int32_t floor_z);
int32_t GetCurrentFloor();

// === COMPONENT CLONING ===
void CloneComponentsFromEntity(EntityId source_id, EntityId target_id, float grid_x, float grid_y, int32_t floor_z);

// Add these helper functions for prototype creation
EntityId GetNextEntityId();
void AddEntityToList(const Entity& entity);

} // namespace simcore