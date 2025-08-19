#include "movement_system.hpp"
#include "../components/component_registry.hpp"
#include "../world/entity.hpp"
#include "../world/world.hpp"
#include <cstdio>
#include <cmath>

namespace simcore {

using EntityId = components::EntityId;

static MovementStats g_stats = {0, 0, 0};

void Movement_Init() {
    g_stats = {0, 0, 0};
    printf("Movement system initialized\n");
}

void Movement_Clear() {
    g_stats = {0, 0, 0};
}

void Movement_Step(float dt) {
    g_stats.total_moving_entities = 0;
    g_stats.entities_moved_this_frame = 0;
    g_stats.chunk_updates_this_frame = 0;
    
    // Query all entities with movement components
    auto entities_with_movement = components::g_movement_components.GetEntitiesWithComponent();
    
    for (EntityId entity_id : entities_with_movement) {
        components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
        components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
        
        if (!movement || !transform) continue;
        
        g_stats.total_moving_entities++;
        
        // Check if entity is actually moving
        bool is_moving = (movement->current_dx != 0.0f || movement->current_dy != 0.0f);
        
        if (is_moving) {
            // Calculate movement delta based on speed and time
            float actual_dx = movement->current_dx * movement->move_speed * dt;
            float actual_dy = movement->current_dy * movement->move_speed * dt;
            
            // Calculate new position
            float new_x = transform->grid_x + actual_dx;
            float new_y = transform->grid_y + actual_dy;
            int32_t new_chunk_x = (int32_t)(new_x / 32.0f);
            int32_t new_chunk_y = (int32_t)(new_y / 32.0f);
            
            // Validate movement (check floor limits)
            if (!CanCreateChunkOnFloor(transform->floor_z, new_chunk_x, new_chunk_y)) {
                // Stop movement if hitting boundary
                movement->current_dx = 0.0f;
                movement->current_dy = 0.0f;
                printf("Entity %d stopped at floor boundary\n", entity_id);
                continue;
            }
            
            // Store old position for chunk mapping
            int32_t old_chunk_x = transform->chunk_x;
            int32_t old_chunk_y = transform->chunk_y;
            int32_t old_floor_z = transform->floor_z;
            
            // Update position
            transform->grid_x = new_x;
            transform->grid_y = new_y;
            transform->chunk_x = new_chunk_x;
            transform->chunk_y = new_chunk_y;
            
            // Mark entity as dirty for rendering
            Entity* entity = GetEntity(entity_id);
            if (entity) {
                entity->is_dirty = true;
            }
            
            g_stats.entities_moved_this_frame++;
            
            // Update chunk mapping if entity crossed chunk boundary
            if (old_chunk_x != new_chunk_x || old_chunk_y != new_chunk_y) {
                UpdateEntityChunkMapping(entity_id, old_chunk_x, old_chunk_y, old_floor_z);
                g_stats.chunk_updates_this_frame++;
            }
        }
    }
}

MovementStats Movement_GetStats() {
    return g_stats;
}

// === MOVEMENT CONTROL FUNCTIONS ===

void Movement_SetVelocity(EntityId entity_id, float dx, float dy) {
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (movement) {
        movement->current_dx = dx;
        movement->current_dy = dy;
        printf("Entity %d velocity set to (%.2f, %.2f)\n", entity_id, dx, dy);
    }
}

void Movement_AddVelocity(EntityId entity_id, float dx, float dy) {
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (movement) {
        movement->current_dx += dx;
        movement->current_dy += dy;
        printf("Entity %d velocity changed by (%.2f, %.2f), now (%.2f, %.2f)\n", 
               entity_id, dx, dy, movement->current_dx, movement->current_dy);
    }
}

void Movement_StopEntity(EntityId entity_id) {
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (movement) {
        movement->current_dx = 0.0f;
        movement->current_dy = 0.0f;
        printf("Entity %d stopped\n", entity_id);
    }
}

void Movement_TeleportEntity(EntityId entity_id, float grid_x, float grid_y) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (!transform) return;
    
    // Store old position for chunk mapping
    int32_t old_chunk_x = transform->chunk_x;
    int32_t old_chunk_y = transform->chunk_y;
    int32_t old_floor_z = transform->floor_z;
    
    // Update position directly (teleport)
    transform->grid_x = grid_x;
    transform->grid_y = grid_y;
    transform->chunk_x = (int32_t)(grid_x / 32.0f);
    transform->chunk_y = (int32_t)(grid_y / 32.0f);
    
    // Mark entity as dirty for rendering
    Entity* entity = GetEntity(entity_id);
    if (entity) {
        entity->is_dirty = true;
    }
    
    // Update chunk mapping if needed
    if (old_chunk_x != transform->chunk_x || old_chunk_y != transform->chunk_y) {
        UpdateEntityChunkMapping(entity_id, old_chunk_x, old_chunk_y, old_floor_z);
    }
    
    printf("Entity %d teleported to (%.1f, %.1f)\n", entity_id, grid_x, grid_y);
}

void Movement_ChangeFloor(EntityId entity_id, int32_t floor_z) {
    components::TransformComponent* transform = components::g_transform_components.GetComponent(entity_id);
    if (!transform) return;
    
    int32_t old_floor_z = transform->floor_z;
    transform->floor_z = floor_z;
    
    // Ensure target floor exists
    Floor* floor = GetFloorByZ(floor_z);
    if (!floor) {
        int32_t chunks_w = (floor_z > 0) ? 2 : 4;
        int32_t chunks_h = (floor_z > 0) ? 2 : 4;
        SpawnFloorAtZ(floor_z, chunks_w, chunks_h, 32, 32);
        printf("Auto-created floor %d for entity movement\n", floor_z);
    }
    
    // Mark entity as dirty for rendering
    Entity* entity = GetEntity(entity_id);
    if (entity) {
        entity->is_dirty = true;
    }
    
    // Update chunk mapping (floor changed)
    UpdateEntityChunkMapping(entity_id, transform->chunk_x, transform->chunk_y, old_floor_z);
    
    printf("Entity %d changed from floor %d to floor %d\n", entity_id, old_floor_z, floor_z);
}

// === MOVEMENT QUERIES ===

bool Movement_IsMoving(EntityId entity_id) {
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (movement) {
        return (movement->current_dx != 0.0f || movement->current_dy != 0.0f);
    }
    return false;
}

void Movement_GetVelocity(EntityId entity_id, float& dx, float& dy) {
    components::MovementComponent* movement = components::g_movement_components.GetComponent(entity_id);
    if (movement) {
        dx = movement->current_dx;
        dy = movement->current_dy;
    } else {
        dx = dy = 0.0f;
    }
}

} // namespace simcore