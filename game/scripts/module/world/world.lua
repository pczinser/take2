-- game/scripts/world.lua
local world = {}

-- Observer configuration
function world.get_observer_config()
    local observers = sim.get_observers()
    if #observers > 0 then
        local observer = observers[1]  -- Use first observer for now
        return {
            hot_chunk_radius = observer.hot_chunk_radius,
            warm_chunk_radius = observer.warm_chunk_radius,
            hot_z_layers = observer.hot_z_layers,
            warm_z_layers = observer.warm_z_layers
        }
    end
    return nil
end

-- Get visual chunks based on observer's current chunk position
function world.get_visual_chunks(floor_z)
    local observers = sim.get_observers()
    local visual_chunks = {}
    
    for _, observer in ipairs(observers) do
        if observer.z == floor_z then
            -- Convert observer tile position to chunk position
            local observer_chunk_x = math.floor(observer.tile_x / 32)
            local observer_chunk_y = math.floor(observer.tile_y / 32)
            
            -- Load chunks based on hot_chunk_radius
            for cx = observer_chunk_x - observer.hot_chunk_radius, observer_chunk_x + observer.hot_chunk_radius do
                for cy = observer_chunk_y - observer.hot_chunk_radius, observer_chunk_y + observer.hot_chunk_radius do
                    table.insert(visual_chunks, {
                        floor_z = floor_z,
                        chunk_x = cx,
                        chunk_y = cy
                    })
                end
            end
        end
    end
    
    return visual_chunks
end

-- Get chunk world position (for Defold positioning)
function world.get_chunk_world_position(chunk_x, chunk_y)
    local chunk_size = 32  -- tiles per chunk
    local tile_size = 64   -- pixels per tile
    return chunk_x * chunk_size * tile_size, chunk_y * chunk_size * tile_size
end

return world
