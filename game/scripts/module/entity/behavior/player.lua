local M = {}

-- Import required modules
local cmd = require("game.scripts.command_queue")
local sim = require("game.scripts.sim")
local bus = require("game.scripts.sim_bus")

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
		-- Get speed from entity.transform (exposed via sim.get_entity)
		local ent = sim.get_entity(self.entity_id)
		local speed = (ent and ent.move_speed) or 6.0
		local moving = (self.move_dx ~= 0 or self.move_dy ~= 0)
		if moving then
			local dt = sim.get_dt and sim.get_dt() or 1/60
			local grid_dx = self.move_dx * speed * dt
			local grid_dy = self.move_dy * speed * dt
			cmd.move_entity(self.entity_id, grid_dx, grid_dy)
			cmd.set_entity_state_flag(self.entity_id, "moving", "true")
			if self.move_dx > 0 then
				cmd.set_entity_state_flag(self.entity_id, "facing", "east")
			elseif self.move_dx < 0 then
				cmd.set_entity_state_flag(self.entity_id, "facing", "west")
			elseif self.move_dy > 0 then
				cmd.set_entity_state_flag(self.entity_id, "facing", "north")
			elseif self.move_dy < 0 then
				cmd.set_entity_state_flag(self.entity_id, "facing", "south")
			end
		else
			cmd.set_entity_state_flag(self.entity_id, "moving", "false")
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
	end
	return false
end

function M.final(self)
	bus.unsubscribe(msg.url())
end

return M
