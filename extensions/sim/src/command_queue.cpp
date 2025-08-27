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
            case CMD_SPAWN_FLOOR_AT_Z:
                ProcessSpawnFloorAtZ(cmd);
                break;
            case CMD_OBSERVER_FOLLOW_ENTITY:
                ProcessObserverFollowEntity(cmd);
                break;
            case CMD_SET_ANIMATION_STATE:
                ProcessSetAnimationState(cmd);
                break;
            case CMD_SET_ENTITY_FACING:
                ProcessSetEntityFacing(cmd);
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

void CommandQueue::ProcessSetAnimationState(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.a = condition key hash
    // cmd.b = condition value hash
    
    const char* key_str = "unknown";
    const char* value_str = "unknown";
    
    // Map common hashes back to strings (this is a simplified approach)
    if (cmd.a == dmHashString64("moving")) key_str = "moving";
    else if (cmd.a == dmHashString64("facing")) key_str = "facing";
    else if (cmd.a == dmHashString64("working")) key_str = "working";
    else if (cmd.a == dmHashString64("powered")) key_str = "powered";
    
    if (cmd.b == dmHashString64("true")) value_str = "true";
    else if (cmd.b == dmHashString64("false")) value_str = "false";
    else if (cmd.b == dmHashString64("north")) value_str = "north";
    else if (cmd.b == dmHashString64("south")) value_str = "south";
    else if (cmd.b == dmHashString64("east")) value_str = "east";
    else if (cmd.b == dmHashString64("west")) value_str = "west";
    
    auto* comp = components::g_animstate_components.GetComponent(cmd.entity_id);
    if (!comp) {
        components::g_animstate_components.AddComponent(cmd.entity_id, components::AnimStateComponent());
        comp = components::g_animstate_components.GetComponent(cmd.entity_id);
    }
    if (!comp) return;
    
    comp->SetCondition(key_str, value_str);
}

void CommandQueue::ProcessSetEntityFacing(const Command& cmd) {
    // cmd.entity_id = entity_id
    // cmd.a = facing direction hash
    
    const char* facing_str = "south";  // default
    
    // Map hash back to string
    if (cmd.a == dmHashString64("north")) facing_str = "north";
    else if (cmd.a == dmHashString64("south")) facing_str = "south";
    else if (cmd.a == dmHashString64("east")) facing_str = "east";
    else if (cmd.a == dmHashString64("west")) facing_str = "west";
    
    auto* comp = components::g_transform_components.GetComponent(cmd.entity_id);
    if (!comp) return;
    
    comp->facing = facing_str;
}

} // namespace simcore