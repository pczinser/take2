#include "world.hpp"
#include <unordered_map>
#include <algorithm>
#include <cstdio>

namespace simcore {
static std::unordered_map<int32_t, Floor> g_floors_by_z;
static std::vector<int32_t> g_floor_z_list;

Floor* GetFloorByZ(int32_t z){ auto it=g_floors_by_z.find(z); return it==g_floors_by_z.end()?nullptr:&it->second; }

bool CanCreateChunkOnFloor(int32_t floor_z, int32_t chunk_x, int32_t chunk_y) {
    if(floor_z == 0) {
        // Ground floor: unlimited chunks
        return true;
    }
    else if(floor_z > 0) {
        // Tower floors: 2x2 chunk limit (chunks 0,0 to 1,1)
        return chunk_x >= 0 && chunk_x <= 1 && chunk_y >= 0 && chunk_y <= 1;
    }
    else {
        // Underground: could be unlimited or limited
        return true; // or implement underground limits
    }
}

int32_t GetFloorMaxChunks(int32_t floor_z) {
    if(floor_z == 0) return -1; // Unlimited
    if(floor_z > 0) return 4;   // 2x2 grid
    return -1; // Underground unlimited (or set limit)
}

int32_t SpawnFloorAtZ(int32_t z, int32_t cw, int32_t ch, int32_t tw, int32_t th) {
    Floor f; 
    f.z = z; 
    f.chunks_w = std::max(1, cw); 
    f.chunks_h = std::max(1, ch); 
    f.tile_w = tw; 
    f.tile_h = th;
    f.max_chunks = GetFloorMaxChunks(z);
    
    // Only create chunks within the floor's limits
    for(int cy = 0; cy < f.chunks_h; ++cy) {
        for(int cx = 0; cx < f.chunks_w; ++cx) {
            if(CanCreateChunkOnFloor(z, cx, cy)) {
                ChunkKey key{(int16_t)cx, (int16_t)cy}; 
                Chunk c; 
                c.w = tw; 
                c.h = th; 
                c.loaded = false; 
                f.chunks.emplace(key, c);
            }
        }
    }
    
    g_floors_by_z.emplace(z, std::move(f)); 
    g_floor_z_list.push_back(z); 
    return z;
}
const std::vector<int32_t>& GetFloorZList(){ return g_floor_z_list; }

// Add helper function to pack tile coordinates
int64_t PackTileKey(int32_t tile_x, int32_t tile_y) {
    return ((int64_t)tile_x << 32) | (int64_t)tile_y;
}

// Add tile getter function
Tile* GetTile(int32_t floor_z, int32_t tile_x, int32_t tile_y) {
    Floor* floor = GetFloorByZ(floor_z);
    if(!floor) {
        printf("GetTile: Floor %d doesn't exist\n", floor_z);
        return nullptr;
    }
    
    int64_t tile_key = PackTileKey(tile_x, tile_y);
    auto it = floor->tiles.find(tile_key);
    if(it != floor->tiles.end()) {
        printf("GetTile: Found existing tile (%d, %d) with %.1f stone\n", tile_x, tile_y, it->second.stone_amount);
        return &it->second;
    }
    
    // Create new tile if it doesn't exist
    printf("GetTile: Creating new tile (%d, %d) with 0.0 stone\n", tile_x, tile_y);
    Tile new_tile;
    new_tile.stone_amount = 0.0f;
    new_tile.iron_amount = 0.0f;
    new_tile.wood_amount = 0.0f;
    new_tile.herbs_amount = 0.0f;
    new_tile.mushrooms_amount = 0.0f;
    new_tile.crystal_amount = 0.0f;
    new_tile.excavated = false;
    
    floor->tiles[tile_key] = new_tile;
    return &floor->tiles[tile_key];
}

// Add tile initialization function
void InitializeTileResources(int32_t floor_z, int32_t tile_x, int32_t tile_y, float stone_amount) {
    // Check if floor exists, create it if it doesn't
    Floor* floor = GetFloorByZ(floor_z);
    if(!floor) {
        printf("Floor %d doesn't exist, creating it...\n", floor_z);
        SpawnFloorAtZ(floor_z, 4, 4, 32, 32); // Create a 4x4 chunk floor
        floor = GetFloorByZ(floor_z);
    }
    
    Tile* tile = GetTile(floor_z, tile_x, tile_y);
    if(tile) {
        tile->stone_amount = stone_amount;
        printf("Initialized tile (%d, %d) on floor %d with %.1f stone\n", tile_x, tile_y, floor_z, stone_amount);
    } else {
        printf("ERROR: Failed to get/create tile (%d, %d) on floor %d\n", tile_x, tile_y, floor_z);
    }
}

} // namespace simcore
