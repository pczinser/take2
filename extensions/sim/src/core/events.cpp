#include "events.hpp"
namespace simcore {
static std::vector<EV_PortalTransit> g_portal_transits;
void Events_Clear(){ g_portal_transits.clear(); }
void Events_Push(const EV_PortalTransit& e){ g_portal_transits.push_back(e); }
const std::vector<EV_PortalTransit>& Events_GetPortalTransits(){ return g_portal_transits; }
} // namespace simcore
