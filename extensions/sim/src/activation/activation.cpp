#include "activation.hpp"
#include "../world/world.hpp"
#include "../observer/observer.hpp"
#include <cmath>
#include <cstdlib>
namespace simcore {
static inline void MarkChunkSets(Floor& f,int32_t ocx,int32_t ocy,
    int32_t hot_cx_r,int32_t hot_cy_r,int32_t warm_cx_r,int32_t warm_cy_r,bool warm_only){
    for(int dy=-warm_cy_r; dy<=warm_cy_r; ++dy){
        for(int dx=-warm_cx_r; dx<=warm_cx_r; ++dx){
            int cx=ocx+dx, cy=ocy+dy;
            if(cx<0||cy<0||cx>=f.chunks_w||cy>=f.chunks_h) continue;
            int64_t key=PackChunkKey((int16_t)cx,(int16_t)cy);
            bool is_hot = !warm_only && (std::abs(dx)<=hot_cx_r) && (std::abs(dy)<=hot_cy_r);
            if(is_hot) f.hot_chunks.insert(key); else f.warm_chunks.insert(key);
            auto it=f.chunks.find(ChunkKey{(int16_t)cx,(int16_t)cy});
            if(it!=f.chunks.end()) it->second.loaded=true;
        }
    }
}
void RebuildActivationUnion(int32_t hotZdef,int32_t warmZdef){
    for(int32_t z: GetFloorZList()){ if(auto* f=GetFloorByZ(z)){ f->hot_chunks.clear(); f->warm_chunks.clear(); } }
    const auto& obs = GetObservers();
    for(const Observer& o: obs){
        auto* base = GetFloorByZ(o.z); if(!base) continue;
        int tiles_cx=base->tile_w, tiles_cy=base->tile_h;
        int hot_cx_r=(o.hot_radius+tiles_cx-1)/tiles_cx;
        int hot_cy_r=(o.hot_radius+tiles_cy-1)/tiles_cy;
        int warm_cx_r=(o.warm_radius+tiles_cx-1)/tiles_cx;
        int warm_cy_r=(o.warm_radius+tiles_cy-1)/tiles_cy;
        int ocx=o.tile_x/tiles_cx, ocy=o.tile_y/tiles_cy;
        int hotZ = o.hot_z_layers>=0?o.hot_z_layers:hotZdef;
        int warmZ= o.warm_z_layers>=0?o.warm_z_layers:warmZdef;
        for(int dz=-hotZ; dz<=hotZ; ++dz){ if(auto* fz=GetFloorByZ(o.z+dz)){
            MarkChunkSets(*fz,ocx,ocy,hot_cx_r,hot_cy_r,hot_cx_r,hot_cy_r,false);
        }}
        for(int dz=-warmZ; dz<=warmZ; ++dz){ if(std::abs(dz)<=hotZ) continue; if(auto* fz=GetFloorByZ(o.z+dz)){
            MarkChunkSets(*fz,ocx,ocy,0,0,warm_cx_r,warm_cy_r,true);
        }}
    }
}
} // namespace simcore
