# Defold Simulation Architecture Plan

## Overview
A deterministic C++ simulation with Lua/Defold handling visuals and input, emphasizing data-first/data-oriented design.

## Core Architecture Principles

### 1. **Separation of Concerns**
- **C++**: Pure simulation logic, deterministic, data-oriented
- **Lua/Defold**: Visuals, input handling, UI
- **No mixing**: Simulation doesn't know about rendering, rendering queries simulation

### 2. **Data-Oriented Design**
- Structure of Arrays (SoA) approach for performance
- Entity data stored in C++, not in Defold game objects
- Templates drive entity creation and behavior

### 3. **Continuous Simulation**
- All entities simulate every frame regardless of visual state
- Chunk system controls rendering only, not simulation
- Visuals can be created/destroyed without affecting simulation

## Entity System

### Entity Structure
```cpp
struct Entity {
    int32_t id;
    EntityType type;
    float grid_x, grid_y;  // Continuous grid coordinates
    int32_t chunk_x, chunk_y;  // Chunk coordinates
    int32_t floor_z;  // Multi-floor support
    std::unordered_map<std::string, float> properties;
    std::unordered_map<std::string, int32_t> int_properties;
    bool is_dirty = false;  // Visual update flag
};
```

### Entity Types
- `ENTITY_PLAYER`: Free-form movement
- `ENTITY_BUILDING`: Grid-snapped, production logic
- `ENTITY_BELT`: Conveyor system (straight, splitter, underground)
- `ENTITY_FACTORY`: Manufacturing buildings
- `ENTITY_PORTAL`: Teleportation between floors/locations

### Data-Driven Templates
```lua
-- Entity templates defined in Lua, registered with C++
entity_templates = {
    player = {
        type = "player",
        properties = { move_speed = 100.0 },
        int_properties = { health = 100 }
    },
    building = {
        type = "building", 
        properties = { production_rate = 1.0 },
        int_properties = { max_storage = 100 }
    }
}
```

## Coordinate Systems

### Three Coordinate Spaces
1. **World Coordinates**: Pixels (64 pixels per grid cell)
2. **Grid Coordinates**: Simulation space (continuous float)
3. **Chunk Coordinates**: Integer chunk indices

### Conversion Functions
```lua
-- Convert world coordinates to grid coordinates
local function world_to_grid(world_x, world_y)
    return world_x / 64.0, world_y / 64.0
end

-- Convert grid coordinates to world coordinates  
local function grid_to_world(grid_x, grid_y)
    return grid_x * 64.0, grid_y * 64.0
end
```

## Chunk System

### Purpose
- **Rendering optimization only** - does NOT control simulation
- **Memory management** - only render visible chunks
- **Performance** - reduce draw calls for off-screen areas

### Chunk Activation
- **Hot chunks**: Player's current chunk + immediate neighbors
- **Warm chunks**: Extended radius around player
- **Cold chunks**: Everything else (no visuals)

### Chunk Data Structure
```cpp
struct Chunk {
    int32_t w{32}, h{32};  // 32x32 grid cells per chunk
    bool loaded{false};
};

struct Floor {
    int32_t z{0};
    int32_t chunks_w{2}, chunks_h{2};
    int32_t tile_w{32}, tile_h{32};
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> chunks;
    std::unordered_set<int64_t> hot_chunks, warm_chunks;
};
```

## Multi-Floor System

### Floor Management
- **Z-depth based rendering**: Each floor uses Z range (floor * 1000 to (floor+1) * 1000)
- **Dynamic chunk rendering**: No static floor collections
- **Portal system**: Teleportation between floors

### Floor Switching
```cpp
static int32_t g_current_floor_z = 0;

void SetCurrentFloor(int32_t floor_z) {
    g_current_floor_z = floor_z;
    // Render script automatically picks up entities in new Z range
}
```

## Rendering System

### Dynamic Chunk Rendering
- **One generic chunk template** reused for all chunks
- **Simulation-driven visuals**: Chunks query simulation for current state
- **Object pooling**: Reuse chunk game objects

### Chunk Lifecycle
1. **Player approaches** → Chunk becomes visible
2. **Chunk manager spawns** → `factory.create("/chunk_template")`
3. **Query simulation** → Get current entities/tiles in chunk
4. **Populate visuals** → Create sprites matching simulation state
5. **Player moves away** → Chunk becomes invisible
6. **Chunk manager despawns** → `go.delete(chunk_go)` (simulation continues)

### Visual Update System
- **Dirty flag**: Mark entities that need visual updates
- **Frustum culling**: Defold's render script handles visibility
- **Efficient updates**: Only update visuals that changed

## Implementation Phases

### Phase 1: Foundation ✅
- [x] Basic entity system with templates
- [x] Coordinate system alignment
- [x] Player movement and chunk tracking
- [x] Multi-floor world structure

### Phase 2: Rendering System
- [ ] Dynamic chunk rendering system
- [ ] Chunk manager with object pooling
- [ ] Visual update system with dirty flags
- [ ] Frustum culling integration

### Phase 3: Game Systems
- [ ] Belt system (straight, splitter, underground)
- [ ] Factory/production system
- [ ] Portal system enhancements
- [ ] Item/fluid systems

### Phase 4: Optimization
- [ ] Performance profiling
- [ ] Memory optimization
- [ ] Rendering batching improvements

## Key Files Structure

### C++ Extension (`extensions/sim/`)
```
src/
├── sim_entry.cpp          # Main entry point, fixed-step simulation
├── world/
│   ├── world.hpp/cpp      # Floor and chunk management
│   └── entity.hpp/cpp     # Entity system and templates
├── lua/
│   └── lua_world.cpp      # Lua bindings for world/entity
└── systems/
    └── portal_system.cpp  # Portal teleportation logic
```

### Lua Scripts (`game/scripts/`)
```
├── main.script            # Global initialization
├── player_controller.script  # Player input and movement
├── chunk_manager.lua      # Dynamic chunk rendering
└── entity_renderer.lua    # Visual update system
```

## Performance Considerations

### Simulation Performance
- **Fixed timestep**: 60 FPS simulation regardless of rendering
- **Data-oriented**: SoA storage for cache efficiency
- **Minimal Lua-C++ calls**: Batch operations where possible

### Rendering Performance
- **Dynamic culling**: Only render visible chunks
- **Object pooling**: Reuse chunk game objects
- **Dirty flag system**: Only update changed visuals
- **Defold batching**: Leverage automatic draw call batching

## Future Considerations

### Scalability
- **Thousands of entities per type**: Belt systems, fluid networks
- **Large worlds**: Multiple floors, extensive areas
- **Complex interactions**: Entity-to-entity communication

### Extensibility
- **New entity types**: Easy to add via templates
- **New systems**: Modular system architecture
- **Visual customization**: Template-driven visuals

---

*This document captures the current architecture decisions and implementation strategy. It will be updated as the system evolves.*
