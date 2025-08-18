local entity_templates = {
    player = {
        type = "player",
        properties = {
            move_speed = 50.0,
            health = 100.0,
            max_health = 100.0
        },
        int_properties = {
            inventory_size = 50
        }
    }
}

-- Register templates with C++
sim.register_entity_templates(entity_templates)
