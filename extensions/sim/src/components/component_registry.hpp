#pragma once
#include "component_manager.hpp"
#include "components.hpp"

namespace simcore {
namespace components {

// Global component managers - one for each component type
extern ComponentManager<MetadataComponent> g_metadata_components;
extern ComponentManager<TransformComponent> g_transform_components;
extern ComponentManager<ProductionComponent> g_production_components;
extern ComponentManager<HealthComponent> g_health_components;
extern ComponentManager<InventoryComponent> g_inventory_components;
extern ComponentManager<AnimStateComponent> g_animstate_components;

// Component system initialization and cleanup
void InitializeComponentSystem();
void ClearComponentSystem();

// Utility functions for component operations
void RemoveAllComponents(EntityId entity_id);
bool HasAnyComponents(EntityId entity_id);

} // namespace components
} // namespace simcore