#include "entity_new.hpp"
#include "world.hpp"
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath>

namespace simcore {

// Entity storage
static std::vector<Entity> g_entities;
static EntityId g_next_entity_id = 1;
static std::unordered_map<std::string, EntityTemplate> g_entity_templates;

// Current floor for queries
static int32_t g_current_floor_z = 0;

// === ENTITY MANAGEMENT ===

EntityId CreateEntity(const std::string& template_name, float grid_x, float grid_y, int32_t floor_z) {
    auto it = g_entity_templates.find(template_name);
    if(it == g_entity_templates.end()) {
        printf("ERROR: Unknown entity template: %s\n", template_name.c_str());
        return -1;
    }
    
    const EntityTemplate& templ = it->second;
    
    // Check if building can be placed (for buildings only)
    if(templ.category == "building" && templ.components.has_building) {
        if(!CanPlaceBuilding(floor_z, (int32_t)grid_x, (int32_t)grid_y, 
                            templ.components.building_width, templ.components.building_height)) {
            printf("ERROR: Cannot place %s at grid (%.1f, %.1f) on floor %d\n", 
                   template_name.c_str(), grid_x, grid_y, floor_z);
            return -1;
        }
    }
    
    // Ensure floor exists
    Floor* floor = GetFloorByZ(floor_z);
    if(!floor) {
        int32_t chunks_w = (floor_z > 0) ? 2 : 4;
        int32_t chunks_h = (floor_z > 0) ? 2 : 4;
        SpawnFloorAtZ(floor_z, chunks_w, chunks_h, 32, 32);
        printf("Auto-created floor %d\n", floor_z);
    }
    
    // Create entity
    EntityId entity_id = g_next_entity_id++;
    Entity entity(entity_id, template_name, template_name);
    
    // Create components from template
    CreateComponentsFromTemplate(entity_id, templ, grid_x, grid_y, floor_z);
    
    g_entities.push_back(entity);
    
    printf("Created entity %d (%s) at grid (%.1f, %.1f) on floor %d\n", 
           entity_id, template_name.c_str(), grid_x, grid_y, floor_z);
    
    return entity_id;
}

Entity* GetEntity(EntityId id) {
    for(auto& entity : g_entities) {
        if(entity.id == id) return &entity;
    }
    return nullptr;
}

void DestroyEntity(EntityId id) {
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

// === TEMPLATE MANAGEMENT ===

void RegisterEntityTemplate(const std::string& name, const EntityTemplate& templ) {
    g_entity_templates[name] = templ;
    printf("Registered entity template: %s\n", name.c_str());
}

EntityTemplate* GetEntityTemplate(const std::string& name) {
    auto it = g_entity_templates.find(name);
    return it != g_entity_templates.end() ? &it->second : nullptr;
}

void ClearEntityTemplates() {
    g_entity_templates.clear();
}

void RegisterDefaultEntityTemplates() {
    // Player template
    EntityTemplate player_template("Player", "player");
    player_template.components.has_metadata = true;
    player_template.components.metadata_display_name = "Player";
    player_template.components.has_movement = true;
    player_template.components.movement_speed = 50.0f;
    player_template.components.has_health = true;
    player_template.components.health_amount = 100;
    player_template.components.has_inventory = true;
    player_template.components.inventory_slots.push_back({0, 50}); // INVENTORY_PLAYER, capacity 50
    RegisterEntityTemplate("player", player_template);
    
    // Building template
    EntityTemplate building_template("Building", "building");
    building_template.components.has_metadata = true;
    building_template.components.metadata_display_name = "Building";
    building_template.components.has_building = true;
    building_template.components.building_width = 1;
    building_template.components.building_height = 1;
    building_template.components.building_type = "building";
    building_template.components.has_health = true;
    building_template.components.health_amount = 100;
    RegisterEntityTemplate("building", building_template);
    
    printf("Registered default entity templates\n");
}

// === COMPONENT CREATION ===

void CreateComponentsFromTemplate(EntityId entity_id, const EntityTemplate& templ, float grid_x, float grid_y, int32_t floor_z) {
    const auto& comp_data = templ.components;
    
    // Always create transform component
    components::g_transform_components.AddComponent(entity_id, 
        components::TransformComponent(grid_x, grid_y, floor_z));
    
    // Create metadata component
    if(comp_data.has_metadata) {
        components::g_metadata_components.AddComponent(entity_id, 
            components::MetadataComponent(comp_data.metadata_display_name));
    }
    
    // Create building component
    if(comp_data.has_building) {
        components::g_building_components.AddComponent(entity_id, 
            components::BuildingComponent(comp_data.building_width, comp_data.building_height, comp_data.building_type));
    }
    
    // Create movement component
    if(comp_data.has_movement) {
        components::g_movement_components.AddComponent(entity_id, 
            components::MovementComponent(comp_data.movement_speed));
    }
    
    // Create production component
    if(comp_data.has_production) {
        components::ProductionComponent prod_comp;
        prod_comp.production_rate = comp_data.production_rate;
        prod_comp.extraction_rate = comp_data.extraction_rate;
        prod_comp.target_resource = comp_data.target_resource;
        components::g_production_components.AddComponent(entity_id, prod_comp);
    }
    
    // Create health component
    if(comp_data.has_health) {
        components::g_health_components.AddComponent(entity_id, 
            components::HealthComponent(comp_data.health_amount));
    }
    
    // Create inventory component and actual inventories
    if(comp_data.has_inventory) {
        std::vector<int32_t> inventory_ids;
        for(const auto& slot : comp_data.inventory_slots) {
            int32_t inv_id = Inventory_Create((InventoryType)slot.inventory_type, slot.capacity);
            inventory_ids.push_back(inv_id);
        }
        components::g_inventory_components.AddComponent(entity_id, 
            components::InventoryComponent(inventory_ids));
    }
    
    printf("Created components for entity %d from template %s\n", entity_id, templ.display_name.c_str());
}

// === SPATIAL QUERIES (using components) ===

std::vector<EntityId> GetEntitiesInChunk(int32_t z, int32_t chunk_x, int32_t chunk_y) {
    std::vector<EntityId> result;
    
    // Query all entities with transform components
    auto entities_with_transform = components::g_transform_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_transform) {
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        if(transform && transform->floor_z == z && 
           transform->chunk_x == chunk_x && transform->chunk_y == chunk_y) {
            result.push_back(entity_id);
        }
    }
    
    return result;
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
        components::BuildingComponent* building = components::g_building_components.GetComponent(entity_id);
        
        if(transform && transform->floor_z == floor_z) {
            int32_t entity_start_x = (int32_t)transform->grid_x;
            int32_t entity_start_y = (int32_t)transform->grid_y;
            int32_t entity_end_x = entity_start_x;
            int32_t entity_end_y = entity_start_y;
            
            if(building) {
                entity_end_x += building->width - 1;
                entity_end_y += building->height - 1;
            }
            
            if(tile_x >= entity_start_x && tile_x <= entity_end_x &&
               tile_y >= entity_start_y && tile_y <= entity_end_y) {
                result.push_back(entity_id);
            }
        }
    }
    
    return result;
}

// === BUILDING PLACEMENT ===

bool CanPlaceBuilding(int32_t floor_z, int32_t base_x, int32_t base_y, int32_t width, int32_t height) {
    // Check floor limits
    if(!CanCreateChunkOnFloor(floor_z, base_x / 32, base_y / 32)) {
        return false;
    }
    
    // Check for overlapping buildings
    auto entities_with_building = components::g_building_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_building) {
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        components::BuildingComponent* building = components::g_building_components.GetComponent(entity_id);
        
        if(transform && building && transform->floor_z == floor_z) {
            int32_t existing_x = (int32_t)transform->grid_x;
            int32_t existing_y = (int32_t)transform->grid_y;
            
            // Check overlap
            if(!(base_x + width <= existing_x || existing_x + building->width <= base_x ||
                 base_y + height <= existing_y || existing_y + building->height <= base_y)) {
                return false; // Buildings overlap
            }
        }
    }
    
    return true;
}

// === SYSTEM INITIALIZATION ===

void InitializeEntitySystem() {
    printf("Entity system: Initializing clean component-based entity system\n");
    
    g_entities.clear();
    g_next_entity_id = 1;
    g_entity_templates.clear();
    g_current_floor_z = 0;
    
    // Initialize component system
    components::InitializeComponentSystem();
    
    // Register default templates
    RegisterDefaultEntityTemplates();
    
    printf("Entity system: Initialized successfully\n");
}

void ClearEntitySystem() {
    printf("Entity system: Clearing all entities and components\n");
    
    // Clear all components
    components::ClearComponentSystem();
    
    // Clear entities
    g_entities.clear();
    g_entity_templates.clear();
    g_next_entity_id = 1;
}

// === FLOOR MANAGEMENT ===

void SetCurrentFloor(int32_t floor_z) {
    g_current_floor_z = floor_z;
    printf("Switched to floor %d\n", floor_z);
}

int32_t GetCurrentFloor() {
    return g_current_floor_z;
}

} // namespace simcore