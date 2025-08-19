#include "inventory_system.hpp"
#include <algorithm>
#include <cstdio>

namespace simcore {

// Inventory data structure (Structure of Arrays for performance)
struct InventorySoA {
    std::vector<InventoryType> types;
    std::vector<int32_t> capacities;
    std::vector<std::unordered_map<ItemType, int32_t>> items;  // item_type -> count
    std::vector<bool> active;
} G;

static int32_t g_next_inventory_id = 1;
static int32_t g_total_items = 0;

void Inventory_Init() {
    G = {};
    g_next_inventory_id = 1;
    g_total_items = 0;
}

void Inventory_Clear() {
    Inventory_Init();
}

InventoryId Inventory_Create(InventoryType type, int32_t capacity) {
    InventoryId id = g_next_inventory_id++;
    
    G.types.push_back(type);
    G.capacities.push_back(capacity);
    G.items.push_back({});  // Empty item map
    G.active.push_back(true);
    
    printf("Inventory system: Created inventory %d (type: %d, capacity: %d)\n", 
           id, (int)type, capacity);
    
    return id;
}

void Inventory_Destroy(InventoryId id) {
    if(id <= 0 || id >= (InventoryId)G.types.size()) return;
    
    // Remove all items from this inventory
    auto& item_map = G.items[id - 1];
    for(const auto& [item_type, count] : item_map) {
        g_total_items -= count;
    }
    
    G.active[id - 1] = false;
    printf("Inventory system: Destroyed inventory %d\n", id);
}

bool Inventory_CanAddItems(InventoryId id, ItemType item, int32_t amount) {
    if(id <= 0 || id >= (InventoryId)G.types.size() || !G.active[id - 1]) return false;
    
    int32_t current_count = Inventory_GetItemCount(id, item);
    int32_t capacity = G.capacities[id - 1];
    
    return (current_count + amount) <= capacity;
}

bool Inventory_AddItems(InventoryId id, ItemType item, int32_t amount) {
    if(!Inventory_CanAddItems(id, item, amount)) return false;
    
    auto& item_map = G.items[id - 1];
    item_map[item] += amount;
    g_total_items += amount;
    
    printf("Inventory system: Added %d items (type: %d) to inventory %d\n", 
           amount, (int)item, id);
    
    return true;
}

bool Inventory_RemoveItems(InventoryId id, ItemType item, int32_t amount) {
    if(id <= 0 || id >= (InventoryId)G.types.size() || !G.active[id - 1]) return false;
    
    auto& item_map = G.items[id - 1];
    auto it = item_map.find(item);
    if(it == item_map.end() || it->second < amount) return false;
    
    it->second -= amount;
    g_total_items -= amount;
    
    // Remove item type if count reaches 0
    if(it->second <= 0) {
        item_map.erase(it);
    }
    
    printf("Inventory system: Removed %d items (type: %d) from inventory %d\n", 
           amount, (int)item, id);
    
    return true;
}

bool Inventory_TransferItems(InventoryId from_id, InventoryId to_id, ItemType item, int32_t amount) {
    if(!Inventory_RemoveItems(from_id, item, amount)) return false;
    if(!Inventory_AddItems(to_id, item, amount)) {
        // Rollback if we can't add to destination
        Inventory_AddItems(from_id, item, amount);
        return false;
    }
    
    printf("Inventory system: Transferred %d items (type: %d) from inventory %d to %d\n", 
           amount, (int)item, from_id, to_id);
    
    return true;
}

int32_t Inventory_GetItemCount(InventoryId id, ItemType item) {
    if(id <= 0 || id >= (InventoryId)G.types.size() || !G.active[id - 1]) return 0;
    
    const auto& item_map = G.items[id - 1];
    auto it = item_map.find(item);
    return it != item_map.end() ? it->second : 0;
}

bool Inventory_HasItems(InventoryId id, ItemType item, int32_t amount) {
    return Inventory_GetItemCount(id, item) >= amount;
}

int32_t Inventory_GetFreeSpace(InventoryId id) {
    if(id <= 0 || id >= (InventoryId)G.types.size() || !G.active[id - 1]) return 0;
    
    int32_t used_space = 0;
    const auto& item_map = G.items[id - 1];
    for(const auto& [item_type, count] : item_map) {
        used_space += count;
    }
    
    return G.capacities[id - 1] - used_space;
}

int32_t Inventory_GetCapacity(InventoryId id) {
    if(id <= 0 || id >= (InventoryId)G.types.size() || !G.active[id - 1]) return 0;
    return G.capacities[id - 1];
}

void Inventory_Step(float dt) {
    // Currently empty - will be used for time-based inventory operations later
    // (like item decay, magic evaporation, etc.)
}

InventoryStats Inventory_GetStats() {
    int32_t total_inventories = 0;
    int32_t total_capacity = 0;
    
    for(size_t i = 0; i < G.types.size(); ++i) {
        if(G.active[i]) {
            total_inventories++;
            total_capacity += G.capacities[i];
        }
    }
    
    return {
        total_inventories,
        g_total_items,
        total_capacity
    };
}

} // namespace simcore
