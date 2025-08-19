#pragma once
#include <cstdint>

namespace simcore {

using EntityId = int32_t;

// Movement system statistics
struct MovementStats {
    int32_t total_moving_entities;
    int32_t entities_moved_this_frame;
    int32_t chunk_updates_this_frame;
};

// Movement system functions
void Movement_Init();
void Movement_Clear();
void Movement_Step(float dt);
MovementStats Movement_GetStats();

// Entity movement control
void Movement_SetVelocity(EntityId entity_id, float dx, float dy);
void Movement_AddVelocity(EntityId entity_id, float dx, float dy);
void Movement_StopEntity(EntityId entity_id);
void Movement_TeleportEntity(EntityId entity_id, float grid_x, float grid_y);
void Movement_ChangeFloor(EntityId entity_id, int32_t floor_z);

// Movement queries
bool Movement_IsMoving(EntityId entity_id);
void Movement_GetVelocity(EntityId entity_id, float& dx, float& dy);

} // namespace simcore