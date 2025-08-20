#include "../systems/extractor_system.hpp"
#include "../components/component_registry.hpp"
#include "../systems/inventory_system.hpp"
#include "../items.hpp"
#include <vector>

namespace simcore {

// EntityId is now in global simcore namespace, so no using declaration needed

static ExtractorStats g_stats = {0, 0, 0};

void Extractor_Init() {
    g_stats = {0, 0, 0};
    printf("Extractor system initialized (component-based)\n");
}

void Extractor_Clear() {
    g_stats = {0, 0, 0};
}

// Replace the entire extractor system with new inventory approach
void Extractor_Step(float dt) {
    auto entities_with_production = components::g_production_components.GetEntitiesWithComponent();
    
    for(EntityId entity_id : entities_with_production) {
        components::ProductionComponent* production = components::g_production_components.GetComponent(entity_id);
        components::InventoryComponent* inventory = components::g_inventory_components.GetComponent(entity_id);
        
        if(production && inventory) {
            // Check if entity has output slots
            std::vector<int32_t> output_slots = Inventory_GetOutputSlots(entity_id);
            
            if(!output_slots.empty()) {
                // Check if any output slot has space
                bool has_space = false;
                for(int32_t slot_index : output_slots) {
                    ItemType current_item = Inventory_GetSlotItem(entity_id, slot_index);
                    int32_t current_quantity = Inventory_GetSlotQuantity(entity_id, slot_index);
                    
                    // Check if slot is empty or can hold more of the target resource
                    if(current_item == ITEM_NONE || 
                       (current_item == (ItemType)production->target_resource && 
                        current_quantity < Items_GetMaxStackSize((ItemType)production->target_resource))) {
                        has_space = true;
                        break;
                    }
                }
                
                if(has_space) {
                    // Add item to first available output slot
                    for(int32_t slot_index : output_slots) {
                        if(Inventory_CanAddToSlot(entity_id, slot_index, (ItemType)production->target_resource, 1)) {
                            Inventory_AddToSlot(entity_id, slot_index, (ItemType)production->target_resource, 1);
                            break;
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