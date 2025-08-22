-- Command Queue Helper Module
-- Provides a clean interface for queuing commands to the C++ simulation

local sim = require("game.scripts.sim")

local command_queue = {}

-- Entity movement
function command_queue.move_entity(entity_id, dx, dy)
	sim.cmd_move_entity(entity_id, dx, dy)
end

-- Entity spawning/destruction
function command_queue.spawn_entity(archetype, x, y, z)
	z = z or 0
	sim.cmd_spawn_entity(hash(archetype), x, y, z)
end

function command_queue.destroy_entity(entity_id)
	sim.cmd_destroy_entity(entity_id)
end

-- Entity positioning
function command_queue.set_entity_position(entity_id, x, y)
	sim.cmd_set_entity_position(entity_id, x, y)
end

function command_queue.set_entity_floor(entity_id, floor)
	sim.cmd_set_entity_floor(entity_id, floor)
end

-- Inventory management
function command_queue.add_item_to_inventory(entity_id, slot, item_type, quantity)
	sim.cmd_add_item_to_inventory(entity_id, slot, item_type, quantity)
end

function command_queue.remove_item_from_inventory(entity_id, slot, quantity)
	sim.cmd_remove_item_from_inventory(entity_id, slot, quantity)
end

-- Observer positioning
function command_queue.set_observer_position(observer_id, floor, x, y)
	sim.cmd_set_observer_position(observer_id, floor, x, y)
end

-- Observer follow
function command_queue.observer_follow_entity(entity_id, observer_id)
	sim.cmd_observer_follow_entity(entity_id, observer_id or 0)
end

-- Entity state flags
function command_queue.set_entity_state_flag(entity_id, flag, value)
	sim.cmd_set_entity_state_flag(entity_id, hash(flag), hash(value))
end

-- Convenience functions for common operations
function command_queue.place_building(archetype, x, y, floor)
	command_queue.spawn_entity(archetype, x, y, floor)
end

function command_queue.move_player(entity_id, dx, dy)
	command_queue.move_entity(entity_id, dx, dy)
end

function command_queue.add_resource_to_inventory(entity_id, resource_type, quantity)
	command_queue.add_item_to_inventory(entity_id, 0, resource_type, quantity)
end

-- Floor spawning
function command_queue.spawn_floor_at_z(z, start_x, start_y, width, height)
	sim.cmd_spawn_floor_at_z(z, start_x, start_y, width, height)
end

return command_queue
