local M = {}

-- Import required modules
local cmd = require("game.scripts.command_queue")
local sim = require("game.scripts.sim")
local bus = require("game.scripts.sim_bus")
M.wants_input = true

-- Convert world coordinates to grid coordinates (Defold system)
local function world_to_grid(world_x, world_y)
	return world_x / 64.0, world_y / 64.0
end

-- Convert grid coordinates to world coordinates (Defold system)
local function grid_to_world(grid_x, grid_y)
	return grid_x * 64.0, grid_y * 64.0
end

local function get_item_name(item_type)
	local item_names = {
		[0] = "None",
		[1] = "Stone",
		[2] = "Iron",
		[3] = "Wood",
		[4] = "Herbs",
		[5] = "Mushrooms",
		[6] = "Crystal",
		[7] = "Cut Stone"
	}
	return item_names[item_type] or "Unknown"
end

local function show_player_stats(self)
	local player = sim.get_entity(self.entity_id)
	if not player then
		print("PLAYER BEHAVIOR: ERROR entity not found")
		return
	end
	print("=== PLAYER STATS ===")
	print(string.format("Position: (%.1f, %.1f)", player.grid_x, player.grid_y))
	print(string.format("Floor: %d", player.floor_z))
	print(string.format("Chunk: (%d, %d)", player.chunk_x, player.chunk_y))
	print(string.format("Entity ID: %d", player.id))
	print(string.format("Move Speed: %.1f", player.move_speed or 100.0))
	print("===================")
end

function M.init(self)
	-- Movement intent only; no local position/floor state
	self.move_dx = 0
	self.move_dy = 0
	bus.subscribe(msg.url())
	-- Deferred entity setup
	timer.delay(0.1, false, function()
		if self.entity_id then
			cmd.add_item_to_inventory(self.entity_id, 0, 1, 5)
			cmd.add_item_to_inventory(self.entity_id, 1, 2, 3)
			cmd.add_item_to_inventory(self.entity_id, 2, 3, 10)
			cmd.observer_follow_entity(self.entity_id)
			-- Set orthographic camera to follow this GO
			local cam_script = msg.url("/camera#script")
			pcall(go.set, cam_script, "follow_target", go.get_id())
			pcall(go.set, cam_script, "follow", true)
			pcall(go.set, cam_script, "follow_lerp", 1.0)
		else
			print("PLAYER BEHAVIOR: ERROR no entity id at init")
		end
	end)
end

function M.on_message(self, message_id, message, sender)
	if message_id == hash("bus_tick") then
		if not self.entity_id then return end
		
		-- Get current transform state
		local transform = sim.get_entity_transform(self.entity_id)
		if not transform then return end
		
		-- Calculate movement state from input
		local moving = (self.move_dx ~= 0 or self.move_dy ~= 0)
		
		-- Calculate facing from input or current transform
		local facing = "south"  -- default
		if self.move_dx > 0 then 
			facing = "east"
		elseif self.move_dx < 0 then 
			facing = "west" 
		elseif self.move_dy > 0 then 
			facing = "north"
		elseif self.move_dy < 0 then 
			facing = "south"
		else 
			facing = transform.facing or "south"  -- keep current facing when not moving
		end
		
		-- Update animation state using new system
		cmd.set_animation_state(self.entity_id, "moving", moving and "true" or "false")
		cmd.set_animation_state(self.entity_id, "facing", facing)
		
		-- Update transform facing if it changed
		if facing ~= transform.facing then
			cmd.set_entity_facing(self.entity_id, facing)
		end
		
		-- Handle movement
		if moving then
			local speed = transform.move_speed or 6.0
			local dt = sim.get_dt and sim.get_dt() or 1/60
			local grid_dx = self.move_dx * speed * dt
			local grid_dy = self.move_dy * speed * dt
			cmd.move_entity(self.entity_id, grid_dx, grid_dy)
		end
	end
end

function M.update(self, dt)
	-- No per-frame movement; visuals update from snapshots
end

function M.on_input(self, action_id, action)
	if action_id == hash("key_w") then
		if action.pressed then self.move_dy = self.move_dy + 1 elseif action.released then self.move_dy = self.move_dy - 1 end
		return true
	elseif action_id == hash("key_s") then
		if action.pressed then self.move_dy = self.move_dy - 1 elseif action.released then self.move_dy = self.move_dy + 1 end
		return true
	elseif action_id == hash("key_a") then
		if action.pressed then self.move_dx = self.move_dx - 1 elseif action.released then self.move_dx = self.move_dx + 1 end
		return true
	elseif action_id == hash("key_d") then
		if action.pressed then self.move_dx = self.move_dx + 1 elseif action.released then self.move_dx = self.move_dx - 1 end
		return true
	elseif action_id == hash("wheel_up") and action.pressed then
		local t = sim.get_entity_transform and sim.get_entity_transform(self.entity_id)
		local new_floor = (t and t.floor_z or 0) + 1
		cmd.set_entity_floor(self.entity_id, new_floor)
		return true
	elseif action_id == hash("wheel_down") and action.pressed then
		local t = sim.get_entity_transform and sim.get_entity_transform(self.entity_id)
		local new_floor = (t and t.floor_z or 0) - 1
		cmd.set_entity_floor(self.entity_id, new_floor)
		return true
	elseif action_id == hash("key_tab") and action.pressed then
		show_player_stats(self)
		return true
	elseif action_id == hash("key_1") and action.pressed then
		-- Spawn decorative statue adjacent to player's facing
		local t = sim.get_entity_transform and sim.get_entity_transform(self.entity_id)
		if t then
			local dx, dy = 0, 0
			local f = t.facing or "south"
			if f == "east" then
				dx = 1
			elseif f == "west" then
				dx = -1
			elseif f == "north" then
				dy = 1
			else
				dy = -1
			end
			-- Convert to integer grid coordinates
			local grid_x = math.floor(t.grid_x or 0)
			local grid_y = math.floor(t.grid_y or 0)
			cmd.spawn_entity("decorative_statue", grid_x + dx, grid_y + dy, t.floor_z or 0)
		end
		return true
	elseif action_id == hash("key_2") and action.pressed then
		-- Spawn extractor adjacent to player's facing (accounting for 3x3 size)
		local t = sim.get_entity_transform and sim.get_entity_transform(self.entity_id)
		if t then
			local dx, dy = 0, 0
			local f = t.facing or "south"
			if f == "east" then
				dx = 2  -- Place 2 tiles away to avoid overlap
			elseif f == "west" then
				dx = -3  -- Place 3 tiles away (building is 3 wide)
			elseif f == "north" then
				dy = 2  -- Place 2 tiles away to avoid overlap
			else
				dy = -3  -- Place 3 tiles away (building is 3 tall)
			end
			-- Convert to integer grid coordinates
			local grid_x = math.floor(t.grid_x or 0)
			local grid_y = math.floor(t.grid_y or 0)
			cmd.spawn_entity("extractor", grid_x + dx, grid_y + dy, t.floor_z or 0)
		end
		return true
	end
	return false
end

function M.final(self)
	bus.unsubscribe(msg.url())
end

return M
