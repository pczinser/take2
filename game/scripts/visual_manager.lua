-- Visual Manager for handling entity visuals and animations
-- Maintains mapping between entity IDs and game objects

local visual_manager = {
	by_entity = {},      -- entity_id -> { go, sprite, visual, current_anim }
	by_url = {},         -- go_url -> entity_id
	prototype_configs = {},
	factory_url = nil
}

-- Animation state flags (keep consistent with C++)
local ANIM_FLAGS = {
	MOVING = 0x01,
	FACING_EAST = 0x02,
	FACING_WEST = 0x04,
	FACING_NORTH = 0x08,
	FACING_SOUTH = 0x10,
	ATTACKING = 0x20,
	HURT = 0x40,
	DEAD = 0x80
}

local world = require("game.scripts.module.world.world")
local sim = require("game.scripts.sim")

-- Local helpers
local function determine_animation(visual_config, moving, facing)
	for anim_name, anim_data in pairs(visual_config.animations) do
		local conditions = anim_data.conditions
		local matches = true
		if conditions.moving ~= nil and conditions.moving ~= moving then
			matches = false
		end
		if conditions.facing ~= nil and conditions.facing ~= facing then
			matches = false
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

local function is_subnormal(v)
	return v ~= 0 and math.abs(v) < 1e-30
end

function visual_manager.init()
	visual_manager.factory_url = "#entity_factory"
	visual_manager.prototype_configs = {
		player = {
			atlas_path = "/asset/atlas/player.atlas",
			layer = 1,
			animations = {
				idle_north = { conditions = { facing = "north", moving = false } },
				idle_south = { conditions = { facing = "south", moving = false } },
				idle_east  = { conditions = { facing = "east",  moving = false } },
				idle_west  = { conditions = { facing = "west",  moving = false } },
				walk_north = { conditions = { facing = "north", moving = true } },
				walk_south = { conditions = { facing = "south", moving = true } },
				walk_east  = { conditions = { facing = "east",  moving = true } },
				walk_west  = { conditions = { facing = "west",  moving = true } },
			}
		},
	}
end

function visual_manager.get_prototype_config(prototype)
	return visual_manager.prototype_configs[prototype]
end

function visual_manager.attach(entity_id, visual_config, position)
	local go_id = factory.create(visual_manager.factory_url, position, nil, { entity_id = entity_id })
	if not go_id then return end
	go.set_position(position, go_id)
	local sprite_url = msg.url(nil, go_id, "sprite")
	local pos = go.get_position(go_id)
	pos.z = visual_config.layer or 1
	go.set_position(pos, go_id)
	visual_manager.by_entity[entity_id] = {
		go = go_id,
		sprite = sprite_url,
		visual = visual_config,
		current_anim = nil,
		position = position,
		last_flags = 0,
		miss = 0,
		missed = 0
	}
	visual_manager.by_url[go_id] = entity_id
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
	local target_pos = vmath.vector3(snapshot_data.x, snapshot_data.y, current_pos.z)
	go.set_position(target_pos, entity_data.go)
	local flags = snapshot_data.flags
	local moving = bit.band(flags, ANIM_FLAGS.MOVING) ~= 0
	local facing = "south"
	if bit.band(flags, ANIM_FLAGS.FACING_NORTH) ~= 0 then 
		facing = "north"
	elseif bit.band(flags, ANIM_FLAGS.FACING_EAST) ~= 0 then 
		facing = "east"
	elseif bit.band(flags, ANIM_FLAGS.FACING_WEST) ~= 0 then 
		facing = "west"
	elseif bit.band(flags, ANIM_FLAGS.FACING_SOUTH) ~= 0 then 
		facing = "south"
	else
		local angle = snapshot_data.facing_angle or 0
		if angle >= -0.785 and angle < 0.785 then
			facing = "east"
		elseif angle >= 0.785 and angle < 2.356 then
			facing = "south"
		elseif angle >= 2.356 or angle < -2.356 then
			facing = "west"
		else
			facing = "north"
		end
	end
	local anim_name = determine_animation(entity_data.visual, moving, facing)
	if anim_name ~= entity_data.current_anim then
		local ok_flipbook = pcall(play_animation, entity_data, anim_name)
		if ok_flipbook then entity_data.current_anim = anim_name end
	end
	entity_data.last_flags = flags
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
	local count = streams.curr.id and #streams.curr.id or 0
	local seen = {}
	local hot_sets = {}

	for i = 1, count do
		local id = streams.curr.id[i]
		local z = streams.curr.z[i]
		local wx = streams.curr.x[i]
		local wy = streams.curr.y[i]
		local flags = streams.curr.flags and streams.curr.flags[i] or 0
		local ang = streams.curr.ang and streams.curr.ang[i] or 0
		local valid = (id and id ~= 0 and is_finite_number(z) and is_finite_number(wx) and is_finite_number(wy))
		if valid then
			local allowed = is_entity_in_hot_chunk(hot_sets, z, wx, wy)
			if not allowed then
				local ent = (sim_api or sim).get_entity(id)
				if ent and ent.prototype_name == "player" then allowed = true
				elseif visual_manager.has_entity(id) then allowed = true end
			end
			local sane = is_finite_number(wx) and is_finite_number(wy) and is_finite_number(z)
				and (not is_subnormal(wx)) and (not is_subnormal(wy)) and (not is_subnormal(z))
			if allowed and sane then
				seen[id] = true
				local ent = (sim_api or sim).get_entity(id)
				if not visual_manager.has_entity(id) then
					if ent and ent.prototype_name then
						local cfg = visual_manager.get_prototype_config(ent.prototype_name)
						if cfg then visual_manager.attach(id, cfg, vmath.vector3(wx, wy, cfg.layer or 1)) end
					end
				end
				visual_manager.update_from_snapshot(id, { x = wx, y = wy, z = z, flags = flags, facing_angle = ang })
			end
		end
	end
	-- No auto-detach here; explicit despawn only
end

-- Explicit attach/detach via bus messages
function visual_manager.on_spawn(message)
	local id = message and message.entity_id
	if not id then return end
	local ent = sim.get_entity(id)
	if not ent or not ent.prototype_name then return end
	local cfg = visual_manager.get_prototype_config(ent.prototype_name)
	if not cfg then return end
	if not visual_manager.has_entity(id) then
		visual_manager.attach(id, cfg, vmath.vector3(message.x or 0, message.y or 0, cfg.layer or 1))
	end
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
