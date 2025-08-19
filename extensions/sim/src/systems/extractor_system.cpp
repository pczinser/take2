#include "../systems/extractor_system.hpp"
#include "../components/component_registry.hpp"
#include "../systems/inventory_system.hpp"
#include <cstdio>

namespace simcore {

using EntityId = components::EntityId;

static ExtractorStats g_stats = {0, 0, 0};

void Extractor_Init() {
    g_stats = {0, 0, 0};
    printf("Extractor system initialized (component-based)\n");
}

void Extractor_Clear() {
    g_stats = {0, 0, 0};
}

void Extractor_Step(float dt) {
    g_stats.total_extractors = 0;
    g_stats.active_extractors = 0;
    
    // Query entities with production components (extractors have production components)
    auto entities_with_production = components::g_production_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_production) {
        components::ProductionComponent* production = components::g_production_components.GetComponent(entity_id);
        components::InventoryComponent* inventory = components::g_inventory_components.GetComponent(entity_id);
        
        // Check if this is an extractor (has extraction_rate > 0)
        if(production && production->extraction_rate > 0.0f) {
            g_stats.total_extractors++;
            
            // Check if extractor has inventory for output
            if(inventory && !inventory->inventory_ids.empty()) {
                g_stats.active_extractors++;
                
                // Update extraction timer
                production->extraction_timer += dt;
                
                // Extract every second (when timer >= 1.0)
                if(production->extraction_timer >= 1.0f) {
                    production->extraction_timer -= 1.0f;  // Reset timer
                    
                    // Try to extract to first inventory (output storage)
                    int32_t output_inventory_id = inventory->inventory_ids[0];
                    
                    if(Inventory_GetFreeSpace(output_inventory_id) > 0) {
                        bool success = Inventory_AddItems(output_inventory_id, (ItemType)production->target_resource, 1);
                        if(success) {
                            g_stats.total_resources_extracted++;
                            printf("Extractor %d extracted 1 resource (type %d) to inventory %d\n", 
                                   entity_id, production->target_resource, output_inventory_id);
                        }
                    } else {
                        printf("Extractor %d output inventory %d is full\n", entity_id, output_inventory_id);
                    }
                }
            }
        }
    }
}

ExtractorStats Extractor_GetStats() {
    return g_stats;
}

} // namespace simcore