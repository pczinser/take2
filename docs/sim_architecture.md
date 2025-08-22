## Simulation Architecture and Lua Integration

This project uses a double-buffered, signal + pull pattern with a centralized command queue. All state mutations occur inside the native C++ simulation; Lua orchestrates via commands, consumes read-only snapshots, and renders visuals.

### High-level flow per tick
- **Command enqueue (Lua):** Lua systems enqueue commands only (no direct state mutation).
- **Process (C++):** Simulation processes commands, advances systems, generates new snapshot buffers.
- **Signal (C++ → Lua):** The engine posts `sim_tick` which is forwarded to the Sim Bus.
- **Pull (Lua):** `sim_bus` reads both snapshot buffers once, stores them, and notifies subscribers. Managers pull the buffers from the bus and update visuals/world.

## Responsibilities by layer

### C++ (native extension)
- Generates snapshots: two buffers: `prev` and `curr` with streams: `id, x, y, z, vx, vy, ang, flags`.
- Processes all commands deterministically at the start of a tick.
- Manages observers; supports auto-follow binding of an observer to an entity.
- Exposes a minimal Lua API for reading snapshots, registering listeners, and sending commands.

Key files:
- `extensions/sim/src/sim_entry.cpp` – main loop, snapshot creation, tick posting.
- `extensions/sim/src/command_queue.{hpp,cpp}` – command definitions and processors.
- `extensions/sim/src/lua/lua_sim.cpp` – Lua bindings to commands and snapshot getters.
- `extensions/sim/src/world/entity.{hpp,cpp}` – entities and prototype registration.

### Sim Bus (Lua)
- `game/scripts/sim_bus.lua` stores the latest `{prev, curr, alpha, tick}` each tick and notifies subscribers via messages:
  - `bus_tick` (no payload)
  - `bus_spawn`, `bus_despawn`, `bus_event` (optional plain-table payloads only)
- Managers subscribe with `msg.url()`; they execute in their own GO context and call `bus.get_snapshots()` to read buffers.

### Managers (Lua)
- Own orchestration and lifecycle per subsystem.
- Subscribe to the bus and on `bus_tick`:
  1) call `bus.get_snapshots()`
  2) extract streams with `buffer.get_stream()`
  3) update visuals/world state (read-only in sim terms)

Example managers:
- `game/scripts/visual_manager.script` → uses `visual_manager.lua` to attach/update/detach entity GOs, drive animations.
- `game/scripts/world_manager.script` → spawns/unloads chunk collections via its local `#chunk_factory` and sets chunk data.

## Command Queue (mutations)

All mutations must go through `game/scripts/command_queue.lua` which wraps native `sim.cmd_*` calls. No Lua code should directly change sim state.

Available commands (Lua wrappers):
- `move_entity(entity_id, dx, dy)`
- `spawn_entity(prototype, x, y, z)`
- `destroy_entity(entity_id)`
- `set_entity_position(entity_id, x, y)`
- `set_entity_floor(entity_id, floor)`
- `add_item_to_inventory(entity_id, slot, item_type, quantity)`
- `remove_item_from_inventory(entity_id, slot, quantity)`
- `set_entity_state_flag(entity_id, flag, value)` – e.g., `("moving","true")`, `("facing","east")`
- `set_observer_position(observer_id, floor, x, y)` – safety net
- `observer_follow_entity(entity_id[, observer_id=0])` – recommended
- `spawn_floor_at_z(z, width, height, start_x, start_y)`

Example usage:
```lua
local cmd = require("game.scripts.command_queue")
cmd.spawn_entity("player", 0, 0, 0)
cmd.observer_follow_entity(player_id)
cmd.set_entity_state_flag(player_id, "moving", "true")
```

## Snapshot access (read-only)

Managers must not call the native snapshot read directly. Instead, they:
1) Subscribe to the bus with `bus.subscribe(msg.url())`
2) On `bus_tick`, call `bus.get_snapshots()`
3) Extract streams from buffers:

```lua
local bus = require("game.scripts.sim_bus")

function on_message(self, id)
  if id == hash("bus_tick") then
    local prev, curr, alpha, tick = bus.get_snapshots()
    if not curr then return end
    local x = buffer.get_stream(curr, hash("x"))
    local y = buffer.get_stream(curr, hash("y"))
    -- update visuals...
  end
end
```

## Prototypes

- Centralized in `game/scripts/module/entity/entity_loader.lua`.
- Registered once at boot; C++ maps `hash(prototype_name) → name` for command-based spawns.
- Terminology is standardized to "prototype" (no "archetype").

## Observer policy

- Preferred: `cmd.observer_follow_entity(player_id)` to bind the default observer.
- Safety: `cmd.set_observer_position(0, floor, x, y)` auto-creates default observer if none exists.
- C++ updates bound observers each tick, no per-frame Lua updates required.

## World and chunks

- `world_manager.script` receives `bus_tick`, calls `world.get_visual_chunks()`, and spawns chunk collections via its own `#chunk_factory`.
- Sends `set_chunk_data` to each chunk collection’s `tilemap` GO to configure floor/chunk coordinates and positions the chunk in world space.

## Naming conventions

- C++ ECS: “System” (e.g., InventorySystem). Lua orchestration: “Manager” (e.g., `visual_manager`). Shared hub: “Bus”.
- Use “prototype” exclusively.
- Keep logic in modules (e.g., `visual_manager.lua`), lifecycle in matching scripts (`visual_manager.script`).

## Do and Don’t

Do:
- Enqueue all mutations via `command_queue.lua`.
- Read snapshots once per tick (via `sim_bus`) and share through `bus.get_snapshots()`.
- Use per-manager scripts to subscribe and act in their own GO context.

Don’t:
- Directly mutate sim state from Lua (no legacy calls like `sim.create_entity`, etc.).
- Send userdata (buffers) through Defold messages.
- Call `sim.read_snapshots()` from managers (bus does that once per tick).

## Deprecations

- `sim.read_snapshot()` shim exists for backward compatibility; prefer `bus.get_snapshots()` in managers and the command queue for all mutations.

## File index (key Lua files)
- `game/scripts/command_queue.lua` – command wrappers.
- `game/scripts/sim_bus.lua` – snapshot store + notifications.
- `game/scripts/visual_manager.lua` – visual logic.
- `game/scripts/visual_manager.script` – lifecycle; subscribes to bus and delegates.
- `game/scripts/world_manager.script` – chunk lifecycle; subscribes to bus.
- `game/scripts/sim.lua` – normalized native sim shim (read/listener/util). 


