#include "command_queue.hpp"
#include "world/entity.hpp"
#include "world/world.hpp"
#include "observer/observer.hpp"
#include "systems/inventory_system.hpp"
#include "core/events.hpp"
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/hash.h>
#include <string>
#include <unordered_map>

namespace simcore {

// Global command queue instance
CommandQueue g_command_queue;

// Static follow bindings: observer_id -> entity_id
static std::unordered_map<int32_t, uint32_t> g_observer_follow_map;

// C API functions for Lua bindings
void EnqueueCommand(const Command& cmd) {
    g_command_queue.Enqueue(cmd);
}

void ProcessCommandQueue(uint32_t current_tick) {
    g_command_queue.ProcessCommands(current_tick);
}

// CommandQueue implementation
void CommandQueue::Enqueue(const Command& cmd) {
    pending_commands.push(cmd);
}

void CommandQueue::ProcessCommands(uint32_t current_tick) {
    while (!pending_commands.empty()) {
        Command cmd = pending_commands.front();
        pending_commands.pop();
        
        // Process based on command type
        switch (cmd.type) {
            case CMD_MOVE_ENTITY:
                ProcessMoveEntity(cmd);
                break;
            case CMD_SPAWN_ENTITY:
                ProcessSpawnEntity(cmd);
                break;
            case CMD_DESTROY_ENTITY:
                ProcessDestroyEntity(cmd);
                break;
            case CMD_SET_ENTITY_POSITION:
                ProcessSetEntityPosition(cmd);
                break;
            case CMD_SET_ENTITY_FLOOR:
                ProcessSetEntityFloor(cmd);
                break;
            case CMD_ADD_ITEM_TO_INVENTORY:
                ProcessAddItemToInventory(cmd);
                break;
            case CMD_REMOVE_ITEM_FROM_INVENTORY:
                ProcessRemoveItemFromInventory(cmd);
                break;
            case CMD_SET_OBSERVER_POSITION:
                ProcessSetObserverPosition(cmd);
                break;
            case CMD_SET_ENTITY_STATE_FLAG:
                ProcessSetEntityStateFlag(cmd);
                break;
            case CMD_SPAWN_FLOOR_AT_Z:
                ProcessSpawnFloorAtZ(cmd);
                break;
            case CMD_OBSERVER_FOLLOW_ENTITY:
                ProcessObserverFollowEntity(cmd);
                break;
            default:
                dmLogWarning("Unknown command type: %u", (unsigned)cmd.type);
                break;
        }
    }

    // After applying all mutations, update any observer that follows an entity
    const auto& observers = GetObservers();
    for (const auto& o : observers) {
        auto it = g_observer_follow_map.find(o.id);
        if (it == g_observer_follow_map.end()) continue;
        EntityId follow_id = (EntityId)it->second;
        auto* t = components::g_transform_components.GetComponent(follow_id);
        if (!t) continue;
        // Keep z from entity floor and tile position from entity grid
        MoveObserver(o.id, t->floor_z, (int32_t)t->grid_x, (int32_t)t->grid_y);
    }
}

void CommandQueue::Clear() {
    while (!pending_commands.empty()) {
        pending_commands.pop();
    }
    g_observer_follow_map.clear();
}

// Command processors
void CommandQueue::ProcessMoveEntity(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.x = dx, cmd.y = dy
    MoveEntity(cmd.entity_id, cmd.x, cmd.y);
}

void CommandQueue::ProcessSpawnEntity(const Command& cmd) {
    // cmd.a = prototype hash
    // cmd.x, cmd.y = position
    // cmd.z = floor (as float, cast to int)
    const char* name = GetPrototypeNameByHash(cmd.a);
    if (name && name[0]) {
        CreateEntity(name, cmd.x, cmd.y, (int32_t)cmd.z);
    } else {
        dmLogWarning("SpawnEntity: unknown prototype hash=%llu", (unsigned long long)cmd.a);
    }
}

void CommandQueue::ProcessDestroyEntity(const Command& cmd) {
    // cmd.entity_id = entity_id
    DestroyEntity(cmd.entity_id);
}

void CommandQueue::ProcessSetEntityPosition(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.x, cmd.y = position
    SetEntityPosition(cmd.entity_id, cmd.x, cmd.y);
}

void CommandQueue::ProcessSetEntityFloor(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.z = floor (as float, cast to int)
    SetEntityFloor(cmd.entity_id, (int32_t)cmd.z);
}

void CommandQueue::ProcessAddItemToInventory(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.a = slot
    // cmd.b = item_type
    // cmd.x = quantity (as float, cast to int)
    Inventory_AddToSlot(cmd.entity_id, (int32_t)cmd.a, (ItemType)cmd.b, (int32_t)cmd.x);
}

void CommandQueue::ProcessRemoveItemFromInventory(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.a = slot
    // cmd.x = quantity (as float, cast to int)
    ItemType item = Inventory_GetSlotItem(cmd.entity_id, (int32_t)cmd.a);
    if (item != ITEM_NONE) {
        Inventory_RemoveFromSlot(cmd.entity_id, (int32_t)cmd.a, item, (int32_t)cmd.x);
    }
}

void CommandQueue::ProcessSetObserverPosition(const Command& cmd) {
    // cmd.entity_id = observer_id (0 means default)
    // cmd.z = floor (as float, cast to int)
    // cmd.x, cmd.y = position (grid coordinates)
    const auto& obs = GetObservers();
    if (cmd.entity_id <= 0) {
        if (obs.empty()) {
            // Create default observer with sensible radii
            SetObserver((int32_t)cmd.z, (int32_t)cmd.x, (int32_t)cmd.y, /*hot*/1, /*warm*/2, /*hotz*/0, /*warmz*/1);
        } else {
            // Move first observer by convention
            MoveObserver(obs.front().id, (int32_t)cmd.z, (int32_t)cmd.x, (int32_t)cmd.y);
        }
        return;
    }
    // Move specified observer if valid
    MoveObserver((int32_t)cmd.entity_id, (int32_t)cmd.z, (int32_t)cmd.x, (int32_t)cmd.y);
}

void CommandQueue::ProcessSetEntityStateFlag(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.a = flag hash
    // cmd.b = value hash
    const uint64_t H_MOVING = dmHashString64("moving");
    const uint64_t H_FACING = dmHashString64("facing");
    const uint64_t H_TRUE   = dmHashString64("true");
    const uint64_t H_FALSE  = dmHashString64("false");
    const uint64_t H_EAST   = dmHashString64("east");
    const uint64_t H_WEST   = dmHashString64("west");
    const uint64_t H_NORTH  = dmHashString64("north");
    const uint64_t H_SOUTH  = dmHashString64("south");

    const uint32_t F_MOVING       = 0x01;
    const uint32_t F_FACING_EAST  = 0x02;
    const uint32_t F_FACING_WEST  = 0x04;
    const uint32_t F_FACING_NORTH = 0x08;
    const uint32_t F_FACING_SOUTH = 0x10;

    auto* comp = components::g_animstate_components.GetComponent(cmd.entity_id);
    if (!comp) {
        components::g_animstate_components.AddComponent(cmd.entity_id, components::AnimStateComponent());
        comp = components::g_animstate_components.GetComponent(cmd.entity_id);
    }
    if (!comp) return;

    if (cmd.a == H_MOVING) {
        if (cmd.b == H_TRUE) comp->flags |= F_MOVING;
        else if (cmd.b == H_FALSE) comp->flags &= ~F_MOVING;
    } else if (cmd.a == H_FACING) {
        comp->flags &= ~(F_FACING_EAST|F_FACING_WEST|F_FACING_NORTH|F_FACING_SOUTH);
        if (cmd.b == H_EAST) comp->flags |= F_FACING_EAST;
        else if (cmd.b == H_WEST) comp->flags |= F_FACING_WEST;
        else if (cmd.b == H_NORTH) comp->flags |= F_FACING_NORTH;
        else if (cmd.b == H_SOUTH) comp->flags |= F_FACING_SOUTH;
    }
}

void CommandQueue::ProcessSpawnFloorAtZ(const Command& cmd) {
    // cmd.z = floor_z (as float, cast to int)
    // cmd.a = width
    // cmd.b = height
    SpawnFloorAtZ((int32_t)cmd.z, (int32_t)cmd.a, (int32_t)cmd.b, 32, 32);
}

void CommandQueue::ProcessObserverFollowEntity(const Command& cmd) {
    // cmd.entity_id = entity_id to follow
    // cmd.a = observer_id (0 means first/default)
    int32_t observer_id = (int32_t)cmd.a;
    const auto& obs = GetObservers();
    if (observer_id <= 0) {
        if (obs.empty()) {
            // Create default observer centered on entity if possible
            auto* t = components::g_transform_components.GetComponent(cmd.entity_id);
            int32_t z = t ? t->floor_z : 0;
            int32_t tx = t ? (int32_t)t->grid_x : 0;
            int32_t ty = t ? (int32_t)t->grid_y : 0;
            observer_id = SetObserver(z, tx, ty, /*hot*/1, /*warm*/2, /*hotz*/0, /*warmz*/1);
        } else {
            observer_id = obs.front().id;
        }
    }
    g_observer_follow_map[observer_id] = cmd.entity_id;
}

} // namespace simcore