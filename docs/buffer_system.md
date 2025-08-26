# Snapshot Buffer System Documentation

## Overview

The snapshot buffer system uses a single contiguous data stream approach for entity snapshots to avoid Defold's multi-stream interleaving issues that caused memory corruption with multiple entities.

## Architecture

### Memory Layout

Each entity occupies `ENTITY_FIELD_COUNT` consecutive floats in the data stream:

```
[id, x, y, z, vx, vy, ang, flags, id, x, y, z, vx, vy, ang, flags, ...]
 |-- Entity 0 --|                   |-- Entity 1 --|
```

### Current Fields (8 floats per entity)

| Index | Field | Type | Description |
|-------|-------|------|-------------|
| 0 | `id` | float | Entity ID (stored as float, convert to uint32 in Lua) |
| 1 | `x` | float | World X coordinate |
| 2 | `y` | float | World Y coordinate |
| 3 | `z` | float | World Z coordinate (floor level) |
| 4 | `vx` | float | X velocity component |
| 5 | `vy` | float | Y velocity component |
| 6 | `ang` | float | Rotation angle (radians) |
| 7 | `flags` | float | Entity flags (stored as float, convert to uint32 in Lua) |

### Access Pattern

**C++**: `datap[entity_index * ENTITY_FIELD_COUNT + field_offset]`

**Lua**: `data_stream[(entity_index - 1) * 8 + field_offset + 1]` (1-based indexing)

## How to Add New Data Fields

### Step 1: Update C++ Constants

In `extensions/sim/src/sim_entry.cpp`:

```cpp
#define ENTITY_FIELD_COUNT 9    // Increment from 8 to 9
#define FIELD_NEW_DATA 8        // Add after FIELD_FLAGS
```

### Step 2: Update Buffer Initialization

In `CreateAndFillSnapshot()`:

```cpp
// Zero-initialize all data to avoid uninitialized values
for (uint32_t i = 0; i < rows; ++i) {
    uint32_t base = i * ENTITY_FIELD_COUNT;
    datap[base + FIELD_ID]       = 0.0f;
    datap[base + FIELD_X]        = 0.0f;
    datap[base + FIELD_Y]        = 0.0f;
    datap[base + FIELD_Z]        = 0.0f;
    datap[base + FIELD_VX]       = 0.0f;
    datap[base + FIELD_VY]       = 0.0f;
    datap[base + FIELD_ANG]      = 0.0f;
    datap[base + FIELD_FLAGS]    = 0.0f;
    datap[base + FIELD_NEW_DATA] = 0.0f;  // Add this line
}
```

### Step 3: Update Entity Filling Loop

In `CreateAndFillSnapshot()`:

```cpp
// Set other fields (TODO: get from actual components)
datap[base + FIELD_VX]       = 0.0f;  // TODO: velocity from physics
datap[base + FIELD_VY]       = 0.0f;  // TODO: velocity from physics
datap[base + FIELD_ANG]      = 0.0f;  // TODO: rotation from transform
datap[base + FIELD_FLAGS]    = 0.0f;  // TODO: entity flags
datap[base + FIELD_NEW_DATA] = get_new_data_value(ent);  // Add this line
```

### Step 4: Update Lua Extraction

In `game/scripts/visual_manager.script`:

```lua
-- Manually extract individual arrays from the data stream
local streams = {
    curr = {
        id = {},
        x = {},
        y = {},
        z = {},
        vx = {},
        vy = {},
        ang = {},
        flags = {},
        new_data = {},  -- Add this line
    }
}

-- Extract data: each entity has 9 consecutive floats (update from 8 to 9)
for i = 1, count do
    local base = (i - 1) * 9  -- Update from 8 to 9
    streams.curr.id[i] = math.floor(data_stream[base + 1])
    streams.curr.x[i] = data_stream[base + 2]
    streams.curr.y[i] = data_stream[base + 3]
    streams.curr.z[i] = data_stream[base + 4]
    streams.curr.vx[i] = data_stream[base + 5]
    streams.curr.vy[i] = data_stream[base + 6]
    streams.curr.ang[i] = data_stream[base + 7]
    streams.curr.flags[i] = math.floor(data_stream[base + 8])
    streams.curr.new_data[i] = data_stream[base + 9]  -- Add this line
end
```

### Step 5: Update Lua Processing

In `game/scripts/visual_manager.lua`:

```lua
local count = #data_stream / 9  -- Update from 8 to 9

for i = 1, count do
    local base = (i - 1) * 9  -- Update from 8 to 9
    local id = math.floor(data_stream[base + 1])
    local wx = data_stream[base + 2]
    local wy = data_stream[base + 3]
    local z = data_stream[base + 4]
    local new_data = data_stream[base + 9]  -- Add this line
    
    -- Use new_data as needed...
end
```

## Why This Approach Works

### Problems with Multi-Stream Approach
- Defold's multi-stream buffers caused interleaving issues
- Memory corruption when accessing multiple entities
- Unpredictable stride calculations
- Buffer alignment problems

### Benefits of Single Stream Approach
- **Contiguous Memory**: All data in one flat array
- **Predictable Layout**: Simple arithmetic indexing
- **No Interleaving**: Bypasses Defold's complex buffer logic
- **Scalable**: Tested with 100+ entities successfully
- **Extensible**: Easy to add new fields

## Performance Considerations

- **Memory Efficiency**: Single allocation instead of multiple streams
- **Cache Locality**: Contiguous memory access patterns
- **Deterministic**: No mysterious buffer corruption
- **Minimal Overhead**: Direct array access without complex stride calculations

## Debugging Tips

- Use `dmLogWarning()` instead of `printf()` for error conditions
- Check entity count: `#data_stream / ENTITY_FIELD_COUNT`
- Validate finite values before writing to buffer
- Test with multiple entities to ensure no corruption
