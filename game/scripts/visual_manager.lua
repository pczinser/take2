-- Visual Manager for handling entity visuals and animations
-- Maintains mapping between entity IDs and game objects

local visual_manager = {
	by_entity = {},
	by_url = {},
	_proto_cache = {},
	factory_url = nil
}

local world = require("game.scripts.module.world.world")
local sim = require("game.scripts.sim")
local bus = require("game.scripts.sim_bus")
local atlas_registry = require("game.scripts.atlas.atlas_registry")

-- Local helpers
local function determine_animation(visual_config, entity_state)
	for anim_name, anim_data in pairs(visual_config.animations) do
		local conditions = anim_data and anim_data.conditions or nil
		local matches = true
		
		if conditions then
			for condition_key, condition_value in pairs(conditions) do
				if entity_state[condition_key] ~= condition_value then
					matches = false
					break
				end
			end
		end
		
		if matches then
			return anim_name
		end
	end
	local first = next(visual_config.animations)
	return first or "idle"
end

local function play_animation(entity_data, anim_name)
	sprite.play_flipbook(entity_data.sprite, hash(anim_name))
end

local function is_finite_number(n)
	return type(n) == "number" and n == n and n ~= math.huge and n ~= -math.huge
end

function visual_manager.init()
	visual_manager.by_entity = visual_manager.by_entity or {}
	visual_manager.by_url = visual_manager.by_url or {}
	visual_manager._proto_cache = visual_manager._proto_cache or {}
	visual_manager.factory_url = visual_manager.factory_url or "#entity_factory"
end

function visual_manager.get_prototype_config(prototype)
	local cached_config = visual_manager._proto_cache[prototype]
	if cached_config then
		return cached_config
	end

	-- Try to find an existing entity of this prototype to get the visual config
	local entities = sim.get_entities_by_prototype(prototype)
	if #entities > 0 then
		local visual_config = sim.get_entity_visual_config(entities[1])
		if visual_config then
			visual_manager._proto_cache[prototype] = visual_config
			return visual_config
		end
	end

	-- Fallback: try to get from any entity that might have this prototype
	for entity_id, _ in pairs(visual_manager.by_entity) do
		local ent = sim.get_entity(entity_id)
		if ent and ent.prototype_name == prototype then
			local visual_config = sim.get_entity_visual_config(entity_id)
			if visual_config then
				visual_manager._proto_cache[prototype] = visual_config
				return visual_config
			end
		end
	end

	return nil
end

function visual_manager.attach(entity_id, visual_config, position)
	print("VISUAL: Attaching entity", entity_id, "with config:", visual_config and visual_config.atlas_path or "nil")
	if visual_config and visual_config.atlas_path then
		print("VISUAL DEBUG: atlas_path type =", type(visual_config.atlas_path))
		print("VISUAL DEBUG: atlas_path value =", visual_config.atlas_path)
	end
	local go_id = factory.create(visual_manager.factory_url, position, nil, { entity_id = entity_id })
	if not go_id then return end
	go.set_position(position, go_id)
	local sprite_url = msg.url(nil, go_id, "sprite")
	
	-- Set the atlas using the registry
	if visual_config and visual_config.atlas_path then
		local success = atlas_registry.set_atlas(sprite_url, visual_config.atlas_path)
		if success then
			print("VISUAL: Set atlas", visual_config.atlas_path, "for entity", entity_id)
		else
			print("VISUAL ERROR: Failed to set atlas", visual_config.atlas_path, "for entity", entity_id)
		end
	end
	
	local pos = go.get_position(go_id)
	pos.z = visual_config.layer or 1
	go.set_position(pos, go_id)
	visual_manager.by_entity[entity_id] = {
		go = go_id,
		sprite = sprite_url,
		visual = visual_config,
		current_anim = nil,
		position = position,
		miss = 0,
		missed = 0
	}
	visual_manager.by_url[go_id] = entity_id
	
	-- Initial animation setting is now handled by behavior modules
end

function visual_manager.detach(entity_id)
	local entity_data = visual_manager.by_entity[entity_id]
	if not entity_data then return end
	go.delete(entity_data.go, true)
	visual_manager.by_entity[entity_id] = nil
	visual_manager.by_url[entity_data.go] = nil
end

function visual_manager.update_from_snapshot(entity_id, snapshot_data)
	local entity_data = visual_manager.by_entity[entity_id]
	if not entity_data then return end
	local current_pos = go.get_position(entity_data.go)
	
	-- Get entity transform to adjust positioning for multi-tile buildings
	local transform = sim.get_entity_transform and sim.get_entity_transform(entity_id)
	local visual_x, visual_y = snapshot_data.x, snapshot_data.y
	
	if transform and (transform.width > 1 or transform.height > 1) then
		-- For multi-tile buildings, center the sprite
		visual_x = snapshot_data.x + (transform.width * 64.0 / 2.0) - 32.0
		visual_y = snapshot_data.y + (transform.height * 64.0 / 2.0) - 32.0
	end
	
	local target_pos = vmath.vector3(visual_x, visual_y, current_pos.z)
	go.set_position(target_pos, entity_data.go)
	
	-- Debug: log visual positioning for multi-tile entities
	if entity_id == 2 or entity_id == 3 then  -- Log for extractor and statue
		print("VISUAL UPDATE: Entity", entity_id, "base (", snapshot_data.x, ",", snapshot_data.y, ") visual (", visual_x, ",", visual_y, ")")
	end
	
	-- Get animation state directly from the new system
	local entity_state = sim.get_entity_animation_state(entity_id) or {}
	
	-- Use the new generic condition matching
	local anim_name = determine_animation(entity_data.visual, entity_state)
	if anim_name ~= entity_data.current_anim then
		local ok_flipbook = pcall(play_animation, entity_data, anim_name)
		if ok_flipbook then entity_data.current_anim = anim_name end
	end
end

local function build_hot_chunk_set_for_floor(floor_z)
	local list = world.get_visual_chunks(floor_z)
	local set = {}
	for i = 1, #list do
		set[string.format("%d_%d", list[i].chunk_x, list[i].chunk_y)] = true
	end
	return set
end

local function is_entity_in_hot_chunk(hot_sets, floor_z, world_x, world_y)
	if not is_finite_number(floor_z) then return false end
	if not is_finite_number(world_x) or not is_finite_number(world_y) then return false end
	local grid_x = world_x / 64.0
	local grid_y = world_y / 64.0
	local cx = math.floor(grid_x / 32)
	local cy = math.floor(grid_y / 32)
	local floor_set = hot_sets[floor_z]
	if not floor_set then
		floor_set = build_hot_chunk_set_for_floor(floor_z)
		hot_sets[floor_z] = floor_set
	end
	return floor_set[string.format("%d_%d", cx, cy)] == true
end

-- Called by visual_manager.script each tick
function visual_manager.on_snapshot_tick(streams, alpha, sim_api)
	local prev, curr, alpha, tick = bus.get_snapshots()
	
	-- Debug: log when we call get_snapshots
	if not curr then
		return
	end
	
	-- Get the data stream
	local data_stream = buffer.get_stream(curr, hash("data"))
	if not data_stream then
		return
	end
	
	local count = #data_stream / 8  -- 8 floats per entity
	
	local hot_sets = {}
	local seen = {}

	for i = 1, count do
		local base = (i - 1) * 8
		local id = math.floor(data_stream[base + 1])  -- id
		local wx = data_stream[base + 2]              -- x
		local wy = data_stream[base + 3]              -- y
		local z = data_stream[base + 4]               -- z
		local valid = (id and id ~= 0 and is_finite_number(z) and is_finite_number(wx) and is_finite_number(wy))
		if valid then
			local visible = is_entity_in_hot_chunk(hot_sets, z, wx, wy)
			
			if visible then
				-- Ensure attached: prefer entity's own visual config, fallback to prototype cache
				if not visual_manager.has_entity(id) then
					local ent = (sim_api or sim).get_entity(id)
					local proto = ent and ent.prototype_name or nil
					local cfg = ((sim_api or sim).get_entity_visual_config and (sim_api or sim).get_entity_visual_config(id)) or nil
					if (not cfg) and proto then
						cfg = visual_manager.get_prototype_config(proto)
					end
					if cfg then
						-- Convert world coordinates back to grid coordinates for consistent positioning
						local grid_x = wx / 64.0
						local grid_y = wy / 64.0
						
						-- Get entity transform to adjust positioning for multi-tile buildings
						local transform = (sim_api or sim).get_entity_transform and (sim_api or sim).get_entity_transform(id)
						local visual_x, visual_y = wx, wy
						
						if transform and (transform.width > 1 or transform.height > 1) then
							-- For multi-tile buildings, center the sprite
							visual_x = wx + (transform.width * 64.0 / 2.0) - 32.0
							visual_y = wy + (transform.height * 64.0 / 2.0) - 32.0
							print("VISUAL DEBUG: Multi-tile entity", id, "base (", wx, ",", wy, ") centered at (", visual_x, ",", visual_y, ")")
						else
							print("VISUAL DEBUG: Single-tile entity", id, "grid coords (", grid_x, ",", grid_y, ") world coords (", wx, ",", wy, ")")
						end
						
						visual_manager.attach(id, cfg, vmath.vector3(visual_x, visual_y, cfg.layer or 1))
					end
				end
				-- Update attached visuals
				if visual_manager.has_entity(id) then
					visual_manager.update_from_snapshot(id, { x = wx, y = wy, z = z })
					seen[id] = true
				end
			end
		end
	end

	-- Detach entities that are no longer visible (left hot chunk zone)
	for entity_id, _ in pairs(visual_manager.by_entity) do
		if not seen[entity_id] then
			print("VISUAL: Detaching entity", entity_id, "- left hot chunk zone")
			visual_manager.detach(entity_id)
		end
	end
end

-- Explicit attach/detach via bus messages
function visual_manager.on_spawn(message)
	-- No-op for visuals; snapshot/visibility governs attach
end

function visual_manager.on_despawn(message)
	local id = message and message.entity_id
	if not id then return end
	visual_manager.detach(id)
end

function visual_manager.cleanup()
	for entity_id, _ in pairs(visual_manager.by_entity) do
		visual_manager.detach(entity_id)
	end
	visual_manager.by_entity = {}
	visual_manager.by_url = {}
end

function visual_manager.has_entity(entity_id)
	return visual_manager.by_entity[entity_id] ~= nil
end

function visual_manager.get_attached_ids()
	local ids = {}
	for id, _ in pairs(visual_manager.by_entity) do
		table.insert(ids, id)
	end
	return ids
end

return visual_manager
