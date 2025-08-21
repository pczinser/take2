-- Visual Manager for handling entity visuals and animations
-- Maintains mapping between entity IDs and game objects

local visual_manager = {
    by_entity = {},      -- entity_id -> { go, sprite, visual, current_anim }
    by_url = {},         -- go_url -> entity_id
    archetype_configs = {},
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

function visual_manager.init()
    print("=== INITIALIZING VISUAL MANAGER ===")
    
    -- Set up factory URL for spawning game objects
    visual_manager.factory_url = "#entity_factory"
    
    -- Initialize archetype configurations
    visual_manager.archetype_configs = {
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

function visual_manager.get_archetype_config(archetype)
    return visual_manager.archetype_configs[archetype]
end

function visual_manager.attach(entity_id, visual_config, position)
    -- Create game object via factory
    local entity_ids = collectionfactory.create(visual_manager.factory_url)
    local go_id = entity_ids[hash("/entity")]
    
    if not go_id then
        print("ERROR: Failed to create entity game object")
        return
    end
    
    -- Set position
    go.set_position(position, go_id)
    
    -- Set up sprite component
    local sprite_url = msg.url(go_id.socket, go_id.path, "sprite")
    
    -- Set atlas once
    go.set(sprite_url, "image", resource.atlas(visual_config.atlas_path))
    
    -- Set layer via z position
    local pos = go.get_position(go_id)
    pos.z = visual_config.layer
    go.set_position(pos, go_id)
    
    -- Store mapping
    visual_manager.by_entity[entity_id] = {
        go = go_id,
        sprite = sprite_url,
        visual = visual_config,
        current_anim = nil,
        position = position,
        last_flags = 0
    }
    
    visual_manager.by_url[go_id] = entity_id
    
    print("Attached visual for entity " .. tostring(entity_id))
end

function visual_manager.detach(entity_id)
    local entity_data = visual_manager.by_entity[entity_id]
    if not entity_data then
        return
    end
    
    -- Delete game object
    go.delete(entity_data.go, true)
    
    -- Clean up mappings
    visual_manager.by_entity[entity_id] = nil
    visual_manager.by_url[entity_data.go] = nil
    
    print("Detached visual for entity " .. tostring(entity_id))
end

function visual_manager.update_from_snapshot(entity_id, snapshot_data)
    local entity_data = visual_manager.by_entity[entity_id]
    if not entity_data then
        return
    end
    
    -- Update position with smoothing
    local target_pos = vmath.vector3(snapshot_data.x, snapshot_data.y, snapshot_data.z)
    local current_pos = go.get_position(entity_data.go)
    local smoothed_pos = vmath.lerp(0.8, current_pos, target_pos)
    go.set_position(smoothed_pos, entity_data.go)
    
    -- Determine animation state from flags
    local flags = snapshot_data.flags
    local moving = bit.band(flags, ANIM_FLAGS.MOVING) ~= 0
    
    -- Determine facing from angle or flags
    local facing = "south" -- default
    if bit.band(flags, ANIM_FLAGS.FACING_NORTH) ~= 0 then 
        facing = "north"
    elseif bit.band(flags, ANIM_FLAGS.FACING_EAST) ~= 0 then 
        facing = "east"
    elseif bit.band(flags, ANIM_FLAGS.FACING_WEST) ~= 0 then 
        facing = "west"
    elseif bit.band(flags, ANIM_FLAGS.FACING_SOUTH) ~= 0 then 
        facing = "south"
    else
        -- Derive facing from angle if no flags set
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
    
    -- Determine which animation to play
    local anim_name = determine_animation(entity_data.visual, moving, facing)
    
    -- Play animation if it changed
    if anim_name ~= entity_data.current_anim then
        play_animation(entity_data, anim_name, facing)
        entity_data.current_anim = anim_name
    end
    
    -- Handle sprite flipping for west-facing
    go.set(entity_data.sprite, "flip_horizontal", facing == "west")
    
    entity_data.last_flags = flags
end

function determine_animation(visual_config, moving, facing)
    -- Find animation that matches current conditions
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
    
    -- Fallback to first animation
    for anim_name, _ in pairs(visual_config.animations) do
        return anim_name
    end
    
    return "idle"
end

function play_animation(entity_data, anim_name, facing)
    -- Play the animation using hash
    sprite.play_flipbook(entity_data.sprite, hash(anim_name))
end

function visual_manager.handle_event(entity_id, event_type, event_data)
    local entity_data = visual_manager.by_entity[entity_id]
    if not entity_data then
        return
    end
    
    -- Handle one-shot events
    if event_type == "attack_start" then
        -- Play attack animation
        sprite.play_flipbook(entity_data.sprite, hash("attack"))
    elseif event_type == "got_hit" then
        -- Play hurt animation
        sprite.play_flipbook(entity_data.sprite, hash("hurt"))
    elseif event_type == "died" then
        -- Play death animation
        sprite.play_flipbook(entity_data.sprite, hash("death"))
    end
end

function visual_manager.cleanup()
    -- Clean up all entities
    for entity_id, _ in pairs(visual_manager.by_entity) do
        visual_manager.detach(entity_id)
    end
    
    visual_manager.by_entity = {}
    visual_manager.by_url = {}
end

return visual_manager
