#pragma once
#include <cstdint>
#include <vector>
namespace simcore {
// Minimal event bus placeholder; expand with variants later
struct EV_PortalTransit {
    uint8_t sys; int32_t id;
    int16_t to_z, to_cx, to_cy, to_tx, to_ty;
};
void Events_Clear();
void Events_Push(const EV_PortalTransit& e);
const std::vector<EV_PortalTransit>& Events_GetPortalTransits();
} // namespace simcore
