-- Clean component-based entity templates

local entity_templates = {
    -- Player entity with movement, health, and inventory
    player = {
        display_name = "Player",
        category = "player",
        components = {
            metadata = {
                display_name = "Player Character"
            },
            movement = {
                move_speed = 50.0
            },
            health = {
                health_amount = 100
            },
            inventory = {
                slots = {
                    { type = 0, capacity = 50 }  -- INVENTORY_PLAYER = 0
                }
            }
        }
    },
    
    -- Stone extractor building
    stone_extractor = {
        display_name = "Stone Extractor",
        category = "building",
        components = {
            metadata = {
                display_name = "Stone Extractor"
            },
            building = {
                width = 2,
                height = 2,
                building_type = "extractor"
            },
            production = {
                extraction_rate = 1.0,
                target_resource = 0  -- ITEM_STONE = 0
            },
            health = {
                health_amount = 150
            },
            inventory = {
                slots = {
                    { type = 2, capacity = 100 }  -- INVENTORY_OUTPUT_SLOT = 2
                }
            }
        }
    },
    
    -- Basic assembler building
    basic_assembler = {
        display_name = "Basic Assembler",
        category = "building", 
        components = {
            metadata = {
                display_name = "Basic Assembler"
            },
            building = {
                width = 3,
                height = 3,
                building_type = "assembler"
            },
            production = {
                production_rate = 2.0
            },
            health = {
                health_amount = 200
            },
            inventory = {
                slots = {
                    { type = 2, capacity = 50 },  -- Input slot
                    { type = 3, capacity = 50 }   -- Output slot
                }
            }
        }
    },
    
    -- Simple decorative building (minimal components)
    decorative_statue = {
        display_name = "Decorative Statue",
        category = "building",
        components = {
            metadata = {
                display_name = "Ancient Statue"
            },
            building = {
                width = 1,
                height = 1,
                building_type = "decoration"
            }
            -- No inventory, no production, no health - just metadata and building
        }
    }
}

-- Register templates with the C++ system
sim.register_entity_templates(entity_templates)