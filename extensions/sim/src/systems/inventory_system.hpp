#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include "../components/component_manager.hpp"
#include "../items.hpp"  // ← Include items instead of defining ItemType here

namespace simcore {

// System functions
void Inventory_Init();
void Inventory_Clear();

// Core operations (work on entity + slot combinations)
bool Inventory_CanAddToSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount);
bool Inventory_AddToSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount);
bool Inventory_RemoveFromSlot(EntityId entity_id, int32_t slot_index, ItemType item, int32_t amount);
bool Inventory_SwapSlots(EntityId entity_id, int32_t slot_a, int32_t slot_b);

// Queries
ItemType Inventory_GetSlotItem(EntityId entity_id, int32_t slot_index);
int32_t Inventory_GetSlotQuantity(EntityId entity_id, int32_t slot_index);
bool Inventory_IsSlotOutput(EntityId entity_id, int32_t slot_index);
std::vector<int32_t> Inventory_GetInputSlots(EntityId entity_id);
std::vector<int32_t> Inventory_GetOutputSlots(EntityId entity_id);

// System update
void Inventory_Step(float dt);

int32_t GetItemMaxStackSize(ItemType item_type);

} // namespace simcore
