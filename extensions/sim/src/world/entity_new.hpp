#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "../components/component_registry.hpp"

namespace simcore {

// Use the same EntityId type as components
using EntityId = components::EntityId;

// Clean EntityTemplate for pure component-based entities
struct EntityTemplate {
    std::string display_name;     // Human-readable name for UI
    std::string category;         // "building", "player", "transport", etc.
    
    // Component data specifications
    struct ComponentData {
        // Metadata component data
        bool has_metadata = false;
        std::string metadata_display_name;
        
        // Transform component data
        bool has_transform = true;  // Almost all entities need transform
        
        // Building component data  
        bool has_building = false;
        int32_t building_width = 1;
        int32_t building_height = 1;
        std::string building_type;
        
        // Movement component data
        bool has_movement = false;
        float movement_speed = 0.0f;
        
        // Production component data
        bool has_production = false;
        float production_rate = 0.0f;
        float extraction_rate = 0.0f;
        int32_t target_resource = -1;
        
        // Health component data
        bool has_health = false;
        int32_t health_amount = 100;
        
        // Inventory component data
        bool has_inventory = false;
        struct InventorySlot {
            int32_t inventory_type;  // InventoryType enum value
            int32_t capacity;
        };
        std::vector<InventorySlot> inventory_slots;
    };
    
    ComponentData components;
    
    EntityTemplate() = default;
    EntityTemplate(const std::string& name, const std::string& cat) 
        : display_name(name), category(cat) {}
};

// Minimal Entity - just core identity, everything else in components
struct Entity {
    EntityId id;
    std::string name;           // For debugging/identification  
    std::string template_name;  // Which template created this entity
    bool is_dirty = false;     // For rendering system
    
    Entity() = default;
    Entity(EntityId entity_id, const std::string& entity_name, const std::string& templ_name)
        : id(entity_id), name(entity_name), template_name(templ_name) {}
};

// === ENTITY MANAGEMENT ===
EntityId CreateEntity(const std::string& template_name, float grid_x, float grid_y, int32_t floor_z);
Entity* GetEntity(EntityId id);
void DestroyEntity(EntityId id);  // Removes entity and all its components
const std::vector<Entity>& GetAllEntities();

// === TEMPLATE MANAGEMENT ===
void RegisterEntityTemplate(const std::string& name, const EntityTemplate& templ);
EntityTemplate* GetEntityTemplate(const std::string& name);
void ClearEntityTemplates();
void RegisterDefaultEntityTemplates();

// === COMPONENT CREATION ===
void CreateComponentsFromTemplate(EntityId entity_id, const EntityTemplate& templ, float grid_x, float grid_y, int32_t floor_z);

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

} // namespace simcore