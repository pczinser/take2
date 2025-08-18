#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simcore {

// Forward declarations
struct Tile;
struct ChunkKey;
struct Chunk;

// World types (define these first)
struct ChunkKey{ int16_t cx{0}, cy{0}; bool operator==(const ChunkKey& o) const {return cx==o.cx&&cy==o.cy;} };
struct ChunkKeyHash{ size_t operator()(const ChunkKey& k) const noexcept { return ((uint32_t)k.cx<<16)^(uint16_t)k.cy; } };
struct Chunk{ int32_t w{32}, h{32}; bool loaded{false}; };

inline int64_t PackChunkKey(int16_t cx,int16_t cy){ return ((int64_t)(uint16_t)cx<<32)|(uint16_t)cy; }

// Resource types enum
enum ResourceType {
    RESOURCE_STONE,
    RESOURCE_IRON,
    RESOURCE_WOOD,
    RESOURCE_HERBS,
    RESOURCE_MUSHROOMS,
    RESOURCE_CRYSTAL
};

// Tile structure
struct Tile {
    float stone_amount = 0.0f;
    float iron_amount = 0.0f;
    float wood_amount = 0.0f;
    float herbs_amount = 0.0f;
    float mushrooms_amount = 0.0f;
    float crystal_amount = 0.0f;
    bool excavated = false;
};

// Floor struct
struct Floor {
    int32_t z{0}; 
    int32_t chunks_w{2}, chunks_h{2}; 
    int32_t tile_w{32}, tile_h{32};
    int32_t max_chunks{4};
    std::unordered_map<ChunkKey,Chunk,ChunkKeyHash> chunks;
    std::unordered_set<int64_t> hot_chunks, warm_chunks;
    
    // Add tile data storage
    std::unordered_map<int64_t, Tile> tiles;  // tile_key -> Tile
};

// Add tile management functions
Tile* GetTile(int32_t floor_z, int32_t tile_x, int32_t tile_y);
void InitializeTileResources(int32_t floor_z, int32_t tile_x, int32_t tile_y, float stone_amount);
int64_t PackTileKey(int32_t tile_x, int32_t tile_y);

// World functions
Floor* GetFloorByZ(int32_t z);
int32_t SpawnFloorAtZ(int32_t z,int32_t cw,int32_t ch,int32_t tw,int32_t th);
const std::vector<int32_t>& GetFloorZList();

// Add floor constraint functions
bool CanCreateChunkOnFloor(int32_t floor_z, int32_t chunk_x, int32_t chunk_y);
int32_t GetFloorMaxChunks(int32_t floor_z);

} // namespace simcore
