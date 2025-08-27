#include "items.hpp"
#include <unordered_map>
#include <cstdio>

namespace simcore {

// Global item registry
static std::unordered_map<ItemType, ItemDefinition> g_item_definitions;

void Items_Init() {
    g_item_definitions.clear();
    printf("Items system initialized\n");
}

void Items_Clear() {
    g_item_definitions.clear();
    printf("Items system cleared\n");
}

void Items_RegisterItem(ItemType type, const std::string& name, int32_t max_stack_size) {
    g_item_definitions[type] = ItemDefinition(name, max_stack_size);
    printf("Registered item: %s (type: %d, stack: %d)\n", name.c_str(), (int)type, max_stack_size);
}

const ItemDefinition* Items_GetDefinition(ItemType type) {
    auto it = g_item_definitions.find(type);
    if (it != g_item_definitions.end()) {
        return &it->second;
    }
    return nullptr;
}

int32_t Items_GetMaxStackSize(ItemType item_type) {
    const ItemDefinition* def = Items_GetDefinition(item_type);
    return def ? def->max_stack_size : 1;
}

// Legacy function for compatibility
int32_t GetItemMaxStackSize(ItemType item_type) {
    return Items_GetMaxStackSize(item_type);
}

} // namespace simcore
