#include "portal_system.hpp"
#include "../core/events.hpp"
#include <unordered_map>
#include <deque>
#include <vector>
namespace simcore {
struct CellKey{ int16_t z,cx,cy,tx,ty; };
static inline uint64_t Pack(const CellKey& k){
  return (uint64_t(uint16_t(k.z))<<48)|(uint64_t(uint16_t(k.cx))<<36)|(uint64_t(uint16_t(k.cy))<<24)|(uint64_t(uint16_t(k.tx))<<12)|uint64_t(uint16_t(k.ty));
}
struct PortalSoA{
  std::vector<int16_t> from_z,from_cx,from_cy,from_tx,from_ty;
  std::vector<int16_t> to_z,to_cx,to_cy,to_tx,to_ty;
  std::vector<int32_t> cooldown_ms,capacity;
  std::vector<int64_t> next_ready_time_ms;
  std::vector<int32_t> inflight;
} G;
static std::unordered_map<uint64_t,std::vector<PortalId>> g_from_index;
static std::deque<PortalRequest> g_requests;
void Portal_Init(){ G={}; g_from_index.clear(); g_requests.clear(); }
void Portal_Clear(){ Portal_Init(); }
PortalId Portal_Add(const PortalDesc& d){
  PortalId id=(PortalId)G.from_z.size();
  G.from_z.push_back(d.from_z); G.from_cx.push_back(d.from_cx); G.from_cy.push_back(d.from_cy); G.from_tx.push_back(d.from_tx); G.from_ty.push_back(d.from_ty);
  G.to_z.push_back(d.to_z); G.to_cx.push_back(d.to_cx); G.to_cy.push_back(d.to_cy); G.to_tx.push_back(d.to_tx); G.to_ty.push_back(d.to_ty);
  G.cooldown_ms.push_back(d.cooldown_ms); G.capacity.push_back(d.capacity);
  G.next_ready_time_ms.push_back(0); G.inflight.push_back(0);
  g_from_index[Pack({d.from_z,d.from_cx,d.from_cy,d.from_tx,d.from_ty})].push_back(id);
  return id;
}
void Portal_Request(const PortalRequest& r){ g_requests.push_back(r); }
static inline bool Ready(PortalId id,int64_t now_ms){
  if(now_ms < G.next_ready_time_ms[id]) return false;
  if(G.capacity[id]==0) return true;
  return G.inflight[id] < G.capacity[id];
}
void Portal_Step(int32_t /*dt_ms*/, int64_t now_ms){
  size_t N=g_requests.size();
  for(size_t i=0;i<N;++i){
    PortalRequest rq=g_requests.front(); g_requests.pop_front();
    auto it=g_from_index.find(Pack({rq.z,rq.cx,rq.cy,rq.tx,rq.ty}));
    if(it==g_from_index.end()) continue;
    for(PortalId pid: it->second){
      if(!Ready(pid, now_ms)) continue;
      // emit a transit event (consumer moves the entity later)
      Events_Push(EV_PortalTransit{rq.sys,rq.id, G.to_z[pid], G.to_cx[pid], G.to_cy[pid], G.to_tx[pid], G.to_ty[pid]});
      if(G.cooldown_ms[pid]>0) G.next_ready_time_ms[pid]=now_ms+G.cooldown_ms[pid];
      if(G.capacity[pid]>0)    ++G.inflight[pid];
      break;
    }
  }
  // instant completion for now
  for(size_t i=0;i<G.inflight.size();++i) G.inflight[i]=0;
}
PortalStats Portal_GetStats(){ return {(int32_t)G.from_z.size(), (int32_t)g_requests.size()}; }
} // namespace simcore
