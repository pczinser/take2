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

function visual_manager.init()
    print("=== INITIALIZING VISUAL MANAGER ===")
    -- Set up factory URL for spawning game objects
    visual_manager.factory_url = "#entity_factory"
    -- Initialize prototype configurations
    visual_manager.prototype_configs = {
        player = {
            atlas_path = "/asset/atlas/player.atlas",
            layer = 1,
            animations = {
                idle_north = { conditions = { facing = "north", moving = false } },
                idle_south = { conditions = { facing = "south", moving = false } },
                idle_east = { conditions = { facing = "east", moving = false } },
                idle_west = { conditions = { facing = "west", moving = false } },
                walk_north = { conditions = { facing = "north", moving = true } },
                walk_south = { conditions = { facing = "south", moving = true } },
                walk_east = { conditions = { facing = "east", moving = true } },
                walk_west = { conditions = { facing = "west", moving = true } }
            }
        },
        
        stone_extractor = {
            atlas_path = "/asset/atlas/buildings.atlas",
            layer = 0,
            animations = {
                idle = { conditions = {} },
                working = { conditions = { working = true } }
            }
        },
        
        crafting_building = {
            atlas_path = "/asset/atlas/buildings.atlas",
            layer = 0,
            animations = {
                idle = { conditions = {} },
                crafting = { conditions = { crafting = true } }
            }
        }
    }
    print("=== VISUAL MANAGER INITIALIZED ===")
end

function visual_manager.get_prototype_config(prototype)
    return visual_manager.prototype_configs[prototype]
end

function visual_manager.attach(entity_id, visual_config, position)
    -- Create game object via factory
    local go_id = factory.create(visual_manager.factory_url, position, nil, { entity_id = entity_id })
    if not go_id then
        print("ERROR: Failed to create entity game object")
        return
    end
    go.set_position(position, go_id)
    local sprite_url = msg.url(nil, go_id, "sprite")
    go.set(sprite_url, "image", resource.atlas(visual_config.atlas_path))
    local pos = go.get_position(go_id)
    pos.z = visual_config.layer
    go.set_position(pos, go_id)
    visual_manager.by_entity[entity_id] = {
        go = go_id,
        sprite = sprite_url,
        visual = visual_config,
        current_anim = nil,
        position = position,
        last_flags = 0
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
    local target_pos = vmath.vector3(snapshot_data.x, snapshot_data.y, snapshot_data.z)
    local current_pos = go.get_position(entity_data.go)
    local smoothed_pos = vmath.lerp(0.8, current_pos, target_pos)
    go.set_position(smoothed_pos, entity_data.go)
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
        play_animation(entity_data, anim_name)
        entity_data.current_anim = anim_name
    end
    go.set(entity_data.sprite, "flip_horizontal", facing == "west")
    entity_data.last_flags = flags
end

-- Helpers for snapshot processing
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

    local entity_data = {}
    for i = 1, count do
        local id = streams.curr.id[i]
        local z = streams.curr.z[i]
        local wx = streams.curr.x[i]
        local wy = streams.curr.y[i]
        local valid = (id and id ~= 0 and is_finite_number(z) and is_finite_number(wx) and is_finite_number(wy))
        if valid and is_entity_in_hot_chunk(hot_sets, z, wx, wy) then
            seen[id] = true
            if not visual_manager.has_entity(id) then
                local ent = (sim_api or sim).get_entity(id)
                if ent and ent.prototype_name then
                    local cfg = visual_manager.get_prototype_config(ent.prototype_name)
                    if cfg then
                        visual_manager.attach(id, cfg, vmath.vector3(wx, wy, z))
                    end
                end
            end
            local x, y
            if streams.prev and streams.prev.x then
                local px = streams.prev.x[i]
                local py = streams.prev.y[i]
                if is_finite_number(px) and is_finite_number(py) then
                    x = px + (wx - px) * (alpha or 0)
                    y = py + (wy - py) * (alpha or 0)
                else
                    x = wx
                    y = wy
                end
            else
                x = wx
                y = wy
            end
            entity_data.x = x
            entity_data.y = y
            entity_data.z = z
            entity_data.vx = streams.curr.vx[i] or 0
            entity_data.vy = streams.curr.vy[i] or 0
            entity_data.facing_angle = streams.curr.ang[i] or 0
            entity_data.flags = streams.curr.flags[i] or 0
            visual_manager.update_from_snapshot(id, entity_data)
        end
    end

    local attached = visual_manager.get_attached_ids()
    for _, id in ipairs(attached) do
        if not seen[id] then
            visual_manager.detach(id)
        end
    end
end

function visual_manager.handle_event(entity_id, event_type, event_data)
    local entity_data = visual_manager.by_entity[entity_id]
    if not entity_data then return end
    if event_type == "attack_start" then
        sprite.play_flipbook(entity_data.sprite, hash("attack"))
    elseif event_type == "got_hit" then
        sprite.play_flipbook(entity_data.sprite, hash("hurt"))
    elseif event_type == "died" then
        sprite.play_flipbook(entity_data.sprite, hash("death"))
    end
end

function visual_manager.cleanup()
    for entity_id, _ in pairs(visual_manager.by_entity) do
        visual_manager.detach(entity_id)
    end
    visual_manager.by_entity = {}
    visual_manager.by_url = {}
end

-- Helpers for manager use
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
