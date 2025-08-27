#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../items.hpp"  // ← Include items instead of inventory system

namespace simcore {
namespace components {

// === CORE COMPONENTS ===

// Metadata component for UI display information
struct MetadataComponent {
    std::string display_name;  // Human-readable name for UI
    std::string category;      // "building", "item", "player", etc.
    std::string description;   // Tooltip text
    
    MetadataComponent() = default;
    MetadataComponent(const std::string& name, const std::string& cat, const std::string& desc)
        : display_name(name), category(cat), description(desc) {}
};

// Transform component for position and spatial data
struct TransformComponent {
    float grid_x, grid_y;      // Grid coordinates
    int32_t floor_z;           // Floor/level
    int32_t chunk_x, chunk_y;  // Chunk coordinates (derived from grid_x/y)
    float move_speed;          // Movement speed
    int32_t width, height;     // Size in grid cells (for buildings/entities that occupy multiple tiles)
    std::string facing;        // Current facing direction ("north", "south", "east", "west")
    
    TransformComponent() : grid_x(0), grid_y(0), floor_z(0), chunk_x(0), chunk_y(0), move_speed(100.0f), width(1), height(1), facing("south") {}
    TransformComponent(float x, float y, int32_t z, float speed = 100.0f, int32_t w = 1, int32_t h = 1) 
        : grid_x(x), grid_y(y), floor_z(z), move_speed(speed), width(w), height(h), facing("south") {
        // Calculate chunk coordinates (assuming 32x32 chunks)
        chunk_x = (int32_t)(x / 32.0f);
        chunk_y = (int32_t)(y / 32.0f);
    }
};

// Production component for entities that produce/extract items
struct ProductionComponent {
    float production_rate;     // Items per second
    float extraction_rate;     // For extractors
    float extraction_timer;    // Internal timer for extraction
    int32_t target_resource;   // What resource to extract (ItemType)
    
    ProductionComponent() : production_rate(0), extraction_rate(0), extraction_timer(0), target_resource(-1) {}
};

// Health component for entities with durability/health
struct HealthComponent {
    int32_t current_health;
    int32_t max_health;
    
    HealthComponent() : current_health(100), max_health(100) {}
    HealthComponent(int32_t health) : current_health(health), max_health(health) {}
    HealthComponent(int32_t current, int32_t max) : current_health(current), max_health(max) {}
};

// Inventory component - NEW DESIGN
struct InventoryComponent {
    struct InventorySlot {
        ItemType item_type;                  // What item is in this slot
        int32_t quantity;                    // How many of that item
        bool is_output;                      // Output flag (vs input)
        std::vector<ItemType> whitelist;     // What items can go in this slot (empty = all allowed)
        
        InventorySlot() : item_type(ITEM_NONE), quantity(0), is_output(false) {}
        InventorySlot(bool output, const std::vector<ItemType>& allowed_items = {}) 
            : item_type(ITEM_NONE), quantity(0), is_output(output), whitelist(allowed_items) {}
    };
    
    std::vector<InventorySlot> slots;
    
    InventoryComponent() = default;
    InventoryComponent(const std::vector<InventorySlot>& slot_list) : slots(slot_list) {}
};

// Animation/state flags used for visuals
struct AnimStateComponent {
    std::unordered_map<std::string, std::string> conditions;  // key -> value pairs
    
    AnimStateComponent() = default;
    
    void SetCondition(const std::string& key, const std::string& value) {
        conditions[key] = value;
    }
    
    std::string GetCondition(const std::string& key, const std::string& default_value = "") const {
        auto it = conditions.find(key);
        return (it != conditions.end()) ? it->second : default_value;
    }
    
    bool HasCondition(const std::string& key) const {
        return conditions.find(key) != conditions.end();
    }
    
    void ClearCondition(const std::string& key) {
        conditions.erase(key);
    }
};

// Visual component for rendering configuration
struct VisualComponent {
    std::string atlas_path;
    int32_t layer;
    
    struct AnimationCondition {
        std::string name;
        std::unordered_map<std::string, std::string> conditions;  // key -> value pairs
        
        AnimationCondition() = default;
        AnimationCondition(const std::string& anim_name) : name(anim_name) {}
    };
    
    std::vector<AnimationCondition> animations;
    
    VisualComponent() : atlas_path(""), layer(1) {}
    VisualComponent(const std::string& atlas, int32_t render_layer) 
        : atlas_path(atlas), layer(render_layer) {}
};

} // namespace components
} // namespace simcore