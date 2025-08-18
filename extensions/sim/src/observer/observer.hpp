#pragma once
#include <cstdint>
#include <vector>
namespace simcore {
struct Observer{
    int32_t id{0}; int32_t z{0}; int32_t tile_x{0}, tile_y{0};
    int32_t hot_radius{32}; int32_t warm_radius{64}; int32_t hot_z_layers{0}; int32_t warm_z_layers{1};
};
int32_t SetObserver(int32_t z,int32_t tx,int32_t ty,int32_t hot,int32_t warm,int32_t hotz,int32_t warmz);
void    MoveObserver(int32_t id,int32_t z,int32_t tx,int32_t ty);
const std::vector<Observer>& GetObservers();
} // namespace simcore
