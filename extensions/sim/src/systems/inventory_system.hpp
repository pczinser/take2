#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace simcore {

// Inventory types
enum InventoryType {
    INVENTORY_PLAYER,
    INVENTORY_EXTRACTOR,
    INVENTORY_INPUT_SLOT,
    INVENTORY_OUTPUT_SLOT,
    INVENTORY_GOLEM,
    INVENTORY_STORAGE,
    INVENTORY_ENEMY,
    INVENTORY_MAGIC_TANK
};

// Item types (we'll expand this later)
enum ItemType {
    ITEM_STONE,
    ITEM_IRON,
    ITEM_WOOD,
    ITEM_HERBS,
    ITEM_MUSHROOMS,
    ITEM_CRYSTAL,
    // Magic items will be added later
};

using InventoryId = int32_t;

// Inventory statistics
struct InventoryStats {
    int32_t total_inventories;
    int32_t total_items;
    int32_t total_capacity;
};

// System functions
void Inventory_Init();
void Inventory_Clear();
InventoryId Inventory_Create(InventoryType type, int32_t capacity);
void Inventory_Destroy(InventoryId id);

// Core operations
bool Inventory_CanAddItems(InventoryId id, ItemType item, int32_t amount);
bool Inventory_AddItems(InventoryId id, ItemType item, int32_t amount);
bool Inventory_RemoveItems(InventoryId id, ItemType item, int32_t amount);
bool Inventory_TransferItems(InventoryId from_id, InventoryId to_id, ItemType item, int32_t amount);

// Queries
int32_t Inventory_GetItemCount(InventoryId id, ItemType item);
bool Inventory_HasItems(InventoryId id, ItemType item, int32_t amount);
int32_t Inventory_GetFreeSpace(InventoryId id);
int32_t Inventory_GetCapacity(InventoryId id);

// System update
void Inventory_Step(float dt);
InventoryStats Inventory_GetStats();

} // namespace simcore
