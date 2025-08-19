#include "../systems/extractor_system.hpp"
#include "../world/entity.hpp"
#include "../world/world.hpp"
#include "../systems/inventory_system.hpp"
#include <cstdio>

namespace simcore {

static ExtractorStats g_stats = {0, 0, 0};

void Extractor_Init() {
    g_stats = {0, 0, 0};
    printf("Extractor system initialized\n");
}

void Extractor_Clear() {
    g_stats = {0, 0, 0};
}

void Extractor_Step(float dt) {
    g_stats.total_extractors = 0;
    g_stats.active_extractors = 0;
    
    const auto& entities = GetAllEntities();  // Can use const version now
    for(auto& entity : const_cast<std::vector<Entity>&>(entities)) {  // Cast to non-const for systems
        if(entity.category == "building" && entity.type == "extractor") {
            g_stats.total_extractors++;
            
            // Check if extractor has required properties
            auto rate_it = entity.properties.find("extraction_rate");
            auto resource_it = entity.int_properties.find("target_resource");
            
            if(rate_it != entity.properties.end() && 
               entity.properties.find("extraction_timer") != entity.properties.end() && 
               resource_it != entity.int_properties.end()) {
                
                g_stats.active_extractors++;
                
                // Get properties
                float rate = rate_it->second;
                float& timer = entity.properties["extraction_timer"];  // Use [] for non-const access
                int32_t target_resource = resource_it->second;
                
                // Update timer
                timer += dt;
                
                // Extract every second (when timer >= 1.0)
                if(timer >= 1.0f) {
                    timer -= 1.0f;  // Reset timer
                    
                    // Check if extractor has inventories
                    if(!entity.inventory_ids.empty()) {
                        int32_t internal_storage_id = entity.inventory_ids[0];
                        
                        // Try to extract 1 resource (simplified for now)
                        if(Inventory_GetFreeSpace(internal_storage_id) > 0) {
                            bool success = Inventory_AddItems(internal_storage_id, (ItemType)target_resource, 1);
                            if(success) {
                                g_stats.total_resources_extracted++;
                                printf("Extractor %d extracted 1 resource (type %d) to inventory %d\n", 
                                       entity.id, target_resource, internal_storage_id);
                            }
                        }
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