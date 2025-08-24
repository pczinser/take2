-- Entity loader module for managing entity prototypes and initialization

local entity_loader = {}

-- Entity prototypes definition
local entity_prototypes = {
    player = {
        components = {
            metadata = {
                display_name = "Player",
                category = "player"
            },
            transform = {
                move_speed = 3.0,
                width = 1,
                height = 1
            },
            health = {
                current_health = 100,
                max_health = 100
            },
            inventory = {
                slots = {
                    -- 10 general slots for player
                    { is_output = false, whitelist = {} },  -- Slot 0
                    { is_output = false, whitelist = {} },  -- Slot 1
                    { is_output = false, whitelist = {} },  -- Slot 2
                    { is_output = false, whitelist = {} },  -- Slot 3
                    { is_output = false, whitelist = {} },  -- Slot 4
                    { is_output = false, whitelist = {} },  -- Slot 5
                    { is_output = false, whitelist = {} },  -- Slot 6
                    { is_output = false, whitelist = {} },  -- Slot 7
                    { is_output = false, whitelist = {} },  -- Slot 8
                    { is_output = false, whitelist = {} },  -- Slot 9
                }
            },
            visual = {
                atlas_path = "/asset/atlas/player.atlas",
                layer = 1,
                -- Animation mapping based on your atlas structure
                animations = {
                    idle_north = {
                        conditions = { facing = "north", moving = false }
                    },
                    idle_south = {
                        conditions = { facing = "south", moving = false }
                    },
                    idle_east = {
                        conditions = { facing = "east", moving = false }
                    },
                    idle_west = {
                        conditions = { facing = "west", moving = false }
                    },
                    -- Reuse idle animations for walking (temporary)
                    walk_north = {
                        conditions = { facing = "north", moving = true }
                    },
                    walk_south = {
                        conditions = { facing = "south", moving = true }
                    },
                    walk_east = {
                        conditions = { facing = "east", moving = true }
                    },
                    walk_west = {
                        conditions = { facing = "west", moving = true }
                    }
                }
            }
        }
    },
    
    crafting_building = {
        components = {
            metadata = {
                display_name = "Crafting Building",
                category = "building"
            },
            transform = {
                move_speed = 0.0,
                width = 1,
                height = 1
            },
            production = {
                production_rate = 10.0
            },
            health = {
                current_health = 100,
                max_health = 100
            },
            inventory = {
                slots = {
                    { is_output = false, whitelist = {1, 3} },  -- Input slot (stone, wood)
                    { is_output = false, whitelist = {2} },      -- Input slot (iron)
                    { is_output = true, whitelist = {7} }        -- Output slot (cut stone)
                }
            }
        }
    },
    
    stone_extractor = {
        components = {
            metadata = {
                display_name = "Stone Extractor",
                category = "building"
            },
            transform = {
                move_speed = 0.0,
                width = 2,
                height = 2
            },
            production = {
                extraction_rate = 1.0,
                target_resource = 1  -- ITEM_STONE
            },
            health = {
                current_health = 150,
                max_health = 150
            },
            inventory = {
                slots = {
                    { is_output = true, whitelist = {1} }  -- Output slot for stone
                }
            }
        }
    },
    
    decorative_statue = {
        components = {
            metadata = {
                display_name = "Ancient Statue",
                category = "building"
            },
            transform = {
                move_speed = 0.0
            },
            -- No inventory, no production, no health - just metadata and building
        }
    }
}

-- Initialize entity system
function entity_loader.init()
    print("=== INITIALIZING ENTITY SYSTEM ===")
    
    -- Register all entity prototypes with the C++ system
    sim.register_entity_prototypes(entity_prototypes)
    
    print("Registered " .. #entity_prototypes .. " entity prototypes")
    print("=== ENTITY SYSTEM INITIALIZED ===")
end

-- Get entity prototype by name
function entity_loader.get_prototype(name)
    return entity_prototypes[name]
end

-- List all available prototypes
function entity_loader.list_prototypes()
    print("=== AVAILABLE ENTITY PROTOTYPES ===")
    for name, prototype in pairs(entity_prototypes) do
        local category = prototype.components.metadata and prototype.components.metadata.category or "unknown"
        print(string.format("  %s (%s)", name, category))
    end
    print("===================================")
end

return entity_loader
