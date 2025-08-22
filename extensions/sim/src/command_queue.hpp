#pragma once
#include <queue>
#include <cstdint>
#include <dmsdk/dlib/hash.h>

namespace simcore {

// Command types
enum CommandType : uint32_t {
    CMD_MOVE_ENTITY = 0,
    CMD_SPAWN_ENTITY,
    CMD_DESTROY_ENTITY,
    CMD_SET_ENTITY_POSITION,
    CMD_SET_ENTITY_FLOOR,
    CMD_ADD_ITEM_TO_INVENTORY,
    CMD_REMOVE_ITEM_FROM_INVENTORY,
    CMD_SET_OBSERVER_POSITION,
    CMD_SET_ENTITY_STATE_FLAG,
    CMD_SPAWN_FLOOR_AT_Z,
    CMD_OBSERVER_FOLLOW_ENTITY,
    CMD_COUNT
};

// Command structure - compact and typed
struct Command {
    CommandType type;
    uint32_t entity_id;  // or observer_id for observer commands
    uint64_t a, b;       // generic parameters (prototype hash, slot, etc.)
    float x, y, z;       // position/floats
    
    Command(CommandType cmd_type, uint32_t id, uint64_t param_a, uint64_t param_b, float pos_x, float pos_y, float pos_z)
        : type(cmd_type), entity_id(id), a(param_a), b(param_b), x(pos_x), y(pos_y), z(pos_z) {}
};

// Command queue class
class CommandQueue {
public:
    // Queue a command for processing at the start of the next tick
    void Enqueue(const Command& cmd);
    
    // Process all queued commands (called at start of each simulation tick)
    void ProcessCommands(uint32_t current_tick);
    
    // Clear all pending commands
    void Clear();
    
    // Get queue size for debugging
    size_t GetQueueSize() const { return pending_commands.size(); }

private:
    std::queue<Command> pending_commands;
    
    // Command processors
    void ProcessMoveEntity(const Command& cmd);
    void ProcessSpawnEntity(const Command& cmd);
    void ProcessDestroyEntity(const Command& cmd);
    void ProcessSetEntityPosition(const Command& cmd);
    void ProcessSetEntityFloor(const Command& cmd);
    void ProcessAddItemToInventory(const Command& cmd);
    void ProcessRemoveItemFromInventory(const Command& cmd);
    void ProcessSetObserverPosition(const Command& cmd);
    void ProcessSetEntityStateFlag(const Command& cmd);
    void ProcessSpawnFloorAtZ(const Command& cmd);
    void ProcessObserverFollowEntity(const Command& cmd);
};

// Global command queue instance
extern CommandQueue g_command_queue;

// C API functions for Lua bindings
void EnqueueCommand(const Command& cmd);
void ProcessCommandQueue(uint32_t current_tick);

} // namespace simcore
