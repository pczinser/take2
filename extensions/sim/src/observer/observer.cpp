#include "observer.hpp"
namespace simcore {
static std::vector<Observer> g_obs; static int32_t g_next=1;
int32_t SetObserver(int32_t z, int32_t tx, int32_t ty, int32_t hot_chunks, int32_t warm_chunks, int32_t hotz, int32_t warmz) {
    // Implementation to use chunk-based radius
    Observer o; o.id=g_next++; o.z=z; o.tile_x=tx; o.tile_y=ty; o.hot_chunk_radius=hot_chunks; o.warm_chunk_radius=warm_chunks; o.hot_z_layers=hotz; o.warm_z_layers=warmz;
    g_obs.push_back(o); return o.id;
}
void MoveObserver(int32_t id,int32_t z,int32_t tx,int32_t ty){ if(id<=0||id>(int)g_obs.size()) return; auto& o=g_obs[id-1]; o.z=z; o.tile_x=tx; o.tile_y=ty; }
const std::vector<Observer>& GetObservers(){ return g_obs; }
} // namespace simcore
