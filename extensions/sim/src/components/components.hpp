#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace simcore {
namespace components {

// === CORE COMPONENTS ===

// Metadata component for UI display information
struct MetadataComponent {
    std::string display_name;  // Human-readable name for UI
    
    MetadataComponent() = default;
    MetadataComponent(const std::string& name) : display_name(name) {}
};

// Transform component for position and spatial data
struct TransformComponent {
    float grid_x, grid_y;      // Grid coordinates
    int32_t floor_z;           // Floor/level
    int32_t chunk_x, chunk_y;  // Chunk coordinates (derived from grid_x/y)
    
    TransformComponent() : grid_x(0), grid_y(0), floor_z(0), chunk_x(0), chunk_y(0) {}
    TransformComponent(float x, float y, int32_t z) 
        : grid_x(x), grid_y(y), floor_z(z) {
        // Calculate chunk coordinates (assuming 32x32 chunks)
        chunk_x = (int32_t)(x / 32.0f);
        chunk_y = (int32_t)(y / 32.0f);
    }
};

// Building component for grid-snapped structures
struct BuildingComponent {
    int32_t width, height;     // Size in grid cells
    std::string building_type; // "extractor", "assembler", etc.
    
    BuildingComponent() : width(1), height(1) {}
    BuildingComponent(int32_t w, int32_t h, const std::string& type) 
        : width(w), height(h), building_type(type) {}
};

// Movement component for entities that can move
struct MovementComponent {
    float move_speed;          // Units per second
    float current_dx, current_dy; // Current movement direction
    
    MovementComponent() : move_speed(0), current_dx(0), current_dy(0) {}
    MovementComponent(float speed) : move_speed(speed), current_dx(0), current_dy(0) {}
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

// Inventory component (keeps existing inventory system integration)
struct InventoryComponent {
    std::vector<int32_t> inventory_ids;  // IDs from existing inventory system
    
    InventoryComponent() = default;
    InventoryComponent(const std::vector<int32_t>& ids) : inventory_ids(ids) {}
};

} // namespace components
} // namespace simcore