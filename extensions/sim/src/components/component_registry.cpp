#include "component_registry.hpp"
#include <cstdio>

namespace simcore {
namespace components {

// Global component manager instances
ComponentManager<MetadataComponent> g_metadata_components;
ComponentManager<TransformComponent> g_transform_components;
ComponentManager<BuildingComponent> g_building_components;
ComponentManager<ProductionComponent> g_production_components;
ComponentManager<HealthComponent> g_health_components;
ComponentManager<InventoryComponent> g_inventory_components;

void InitializeComponentSystem() {
    printf("Component system: Initializing component managers\n");
    
    // Clear all component managers
    g_metadata_components.Clear();
    g_transform_components.Clear();
    g_building_components.Clear();
    g_production_components.Clear();
    g_health_components.Clear();
    g_inventory_components.Clear();
    
    printf("Component system: Initialized successfully\n");
}

void ClearComponentSystem() {
    printf("Component system: Clearing all components\n");
    
    g_metadata_components.Clear();
    g_transform_components.Clear();
    g_building_components.Clear();
    g_production_components.Clear();
    g_health_components.Clear();
    g_inventory_components.Clear();
}

void RemoveAllComponents(EntityId entity_id) {
    g_metadata_components.RemoveComponent(entity_id);
    g_transform_components.RemoveComponent(entity_id);
    g_building_components.RemoveComponent(entity_id);
    g_production_components.RemoveComponent(entity_id);
    g_health_components.RemoveComponent(entity_id);
    g_inventory_components.RemoveComponent(entity_id);
}

bool HasAnyComponents(EntityId entity_id) {
    return g_metadata_components.HasComponent(entity_id) ||
           g_transform_components.HasComponent(entity_id) ||
           g_building_components.HasComponent(entity_id) ||
           g_production_components.HasComponent(entity_id) ||
           g_health_components.HasComponent(entity_id) ||
           g_inventory_components.HasComponent(entity_id);
}

} // namespace components
} // namespace simcore