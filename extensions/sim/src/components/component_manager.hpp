#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace simcore {
namespace components {

using EntityId = int32_t;

// Template-based component manager for type-safe, cache-friendly storage
template<typename T>
class ComponentManager {
private:
    std::vector<T> components;                              // Dense array for cache performance
    std::unordered_map<EntityId, size_t> entity_to_index;  // Entity ID -> component index
    std::unordered_map<size_t, EntityId> index_to_entity;  // Component index -> Entity ID
    std::vector<size_t> free_indices;                      // Reusable component slots

public:
    // Add a component to an entity
    void AddComponent(EntityId entity_id, const T& component) {
        // Remove existing component if present
        RemoveComponent(entity_id);
        
        size_t index;
        if (!free_indices.empty()) {
            // Reuse a free slot
            index = free_indices.back();
            free_indices.pop_back();
            components[index] = component;
        } else {
            // Add new component
            index = components.size();
            components.push_back(component);
        }
        
        entity_to_index[entity_id] = index;
        index_to_entity[index] = entity_id;
    }
    
    // Get component for an entity (returns nullptr if not found)
    T* GetComponent(EntityId entity_id) {
        auto it = entity_to_index.find(entity_id);
        if (it != entity_to_index.end()) {
            return &components[it->second];
        }
        return nullptr;
    }
    
    // Check if entity has this component
    bool HasComponent(EntityId entity_id) const {
        return entity_to_index.find(entity_id) != entity_to_index.end();
    }
    
    // Remove component from entity
    void RemoveComponent(EntityId entity_id) {
        auto it = entity_to_index.find(entity_id);
        if (it != entity_to_index.end()) {
            size_t index = it->second;
            
            // Mark slot as free
            free_indices.push_back(index);
            
            // Remove mappings
            entity_to_index.erase(it);
            index_to_entity.erase(index);
        }
    }
    
    // Get all entities that have this component
    std::vector<EntityId> GetEntitiesWithComponent() const {
        std::vector<EntityId> entities;
        entities.reserve(entity_to_index.size());
        
        for (const auto& [entity_id, index] : entity_to_index) {
            entities.push_back(entity_id);
        }
        
        return entities;
    }
    
    // Get total number of components
    size_t GetComponentCount() const {
        return entity_to_index.size();
    }
    
    // Clear all components
    void Clear() {
        components.clear();
        entity_to_index.clear();
        index_to_entity.clear();
        free_indices.clear();
    }
};

} // namespace components
} // namespace simcore