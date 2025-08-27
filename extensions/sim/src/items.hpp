#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace simcore {

// Item types (we'll expand this later)
enum ItemType {
    ITEM_NONE = 0,
    ITEM_STONE,
    ITEM_IRON,
    ITEM_WOOD,
    ITEM_HERBS,
    ITEM_MUSHROOMS,
    ITEM_CRYSTAL,
    ITEM_CUT_STONE,
    // Magic items will be added later
};

// Item definitions
struct ItemDefinition {
    std::string name;
    int32_t max_stack_size;
    
    ItemDefinition() = default;
    ItemDefinition(const std::string& item_name, int32_t stack_size) 
        : name(item_name), max_stack_size(stack_size) {}
};

// Item registry functions
void Items_Init();
void Items_Clear();
void Items_RegisterItem(ItemType type, const std::string& name, int32_t max_stack_size);
const ItemDefinition* Items_GetDefinition(ItemType type);
int32_t Items_GetMaxStackSize(ItemType item_type);

} // namespace simcore
