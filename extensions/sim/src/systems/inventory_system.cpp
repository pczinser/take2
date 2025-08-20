#include "inventory_system.hpp"
#include "../components/component_registry.hpp"
#include "../items.hpp"
#include <cstdio>

namespace simcore {

// Remove this line - it's already in component_registry.cpp
// components::ComponentManager<components::InventoryComponent> g_inventory_components;

void Inventory_Init() {
    // Register default items
    Items_RegisterItem(ITEM_STONE, "Stone", 64);
    Items_RegisterItem(ITEM_IRON, "Iron", 32);
    Items_RegisterItem(ITEM_WOOD, "Wood", 50);
    Items_RegisterItem(ITEM_HERBS, "Herbs", 20);
    Items_RegisterItem(ITEM_MUSHROOMS, "Mushrooms", 10);
    Items_RegisterItem(ITEM_CRYSTAL, "Crystal", 1);
    Items_RegisterItem(ITEM_CUT_STONE, "Cut Stone", 16);
    
    printf("Inventory system initialized\n");
}

void Inventory_Clear() {
    // Use the global component manager instead
    components::g_inventory_components.Clear();
    printf("Inventory system cleared\n");
}

// Update all functions to use the global component manager
bool Inventory_CanAddToSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_index >= (int32_t)inventory->slots.size()) {
        return false;
    }
    
    auto& slot = inventory->slots[slot_index];
    
    // Check whitelist
    if (!slot.whitelist.empty()) {
        bool allowed = false;
        for (ItemType allowed_item : slot.whitelist) {
            if (allowed_item == item) {
                allowed = true;
                break;
            }
        }
        if (!allowed) return false;
    }
    
    // Check if slot is empty or has same item
    if (slot.item_type != ITEM_NONE && slot.item_type != item) {
        return false;
    }
    
    // Check stack size limit
    int32_t max_stack = Items_GetMaxStackSize(item);
    if (slot.quantity + amount > max_stack) {
        return false;
    }
    
    return true;
}

bool Inventory_AddToSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount) {
    if (!Inventory_CanAddToSlot(entity_id, slot_index, item, amount)) {
        return false;
    }
    
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    auto& slot = inventory->slots[slot_index];
    
    if (slot.item_type == ITEM_NONE) {
        slot.item_type = item;
        slot.quantity = amount;
    } else {
        slot.quantity += amount;
    }
    
    return true;
}

bool Inventory_RemoveFromSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_index >= (int32_t)inventory->slots.size()) {
        return false;
    }
    
    auto& slot = inventory->slots[slot_index];
    if (slot.item_type != item || slot.quantity < amount) {
        return false;
    }
    
    slot.quantity -= amount;
    if (slot.quantity == 0) {
        slot.item_type = ITEM_NONE;
    }
    
    return true;
}

bool Inventory_SwapSlots(EntityId entity_id, int32_t slot_a, int32_t slot_b) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_a >= (int32_t)inventory->slots.size() || slot_b >= (int32_t)inventory->slots.size()) {
        return false;
    }
    
    std::swap(inventory->slots[slot_a], inventory->slots[slot_b]);
    return true;
}

// Queries
ItemType Inventory_GetSlotItem(EntityId entity_id, int32_t slot_index) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_index >= (int32_t)inventory->slots.size()) {
        return ITEM_NONE;
    }
    return inventory->slots[slot_index].item_type;
}

int32_t Inventory_GetSlotQuantity(EntityId entity_id, int32_t slot_index) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_index >= (int32_t)inventory->slots.size()) {
        return 0;
    }
    return inventory->slots[slot_index].quantity;
}

bool Inventory_IsSlotOutput(EntityId entity_id, int32_t slot_index) {
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory || slot_index >= (int32_t)inventory->slots.size()) {
        return false;
    }
    return inventory->slots[slot_index].is_output;
}

std::vector<int32_t> Inventory_GetInputSlots(EntityId entity_id) {
    std::vector<int32_t> result;
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory) return result;
    
    for (int32_t i = 0; i < (int32_t)inventory->slots.size(); i++) {
        if (!inventory->slots[i].is_output) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<int32_t> Inventory_GetOutputSlots(EntityId entity_id) {
    std::vector<int32_t> result;
    auto* inventory = components::g_inventory_components.GetComponent(entity_id);
    if (!inventory) return result;
    
    for (int32_t i = 0; i < (int32_t)inventory->slots.size(); i++) {
        if (inventory->slots[i].is_output) {
            result.push_back(i);
        }
    }
    return result;
}

// System update
void Inventory_Step(float dt) {
    // Inventory system doesn't need per-frame updates
    // It's purely data-driven
}

} // namespace simcore
