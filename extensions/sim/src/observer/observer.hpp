#pragma once
#include <cstdint>
#include <vector>
namespace simcore {
struct Observer{
    int32_t id{0}; 
    int32_t z{0}; 
    int32_t tile_x{0}, tile_y{0};  // Keep tile position for observer
    int32_t hot_chunk_radius{1};   // 1 chunk radius = 3x3 grid = 9 chunks total
    int32_t warm_chunk_radius{2};  // 2 chunk radius = 5x5 grid = 25 chunks total
    int32_t hot_z_layers{0}; 
    int32_t warm_z_layers{1};
};
int32_t SetObserver(int32_t z,int32_t tx,int32_t ty,int32_t hot_chunks,int32_t warm_chunks,int32_t hotz,int32_t warmz);
void    MoveObserver(int32_t id,int32_t z,int32_t tx,int32_t ty);
const std::vector<Observer>& GetObservers();
} // namespace simcore
