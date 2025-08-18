#pragma once
#include <cstdint>
namespace simcore {
struct PortalDesc{
  int16_t from_z, from_cx, from_cy, from_tx, from_ty;
  int16_t to_z,   to_cx,   to_cy,   to_tx,   to_ty;
  int32_t cooldown_ms; int32_t capacity;
};
using PortalId = int32_t;
struct PortalRequest{ uint8_t sys; int32_t id; int16_t z,cx,cy,tx,ty; };
struct PortalStats{ int32_t count; int32_t pending; };
void Portal_Init();
void Portal_Clear();
PortalId Portal_Add(const PortalDesc& d);
void Portal_Request(const PortalRequest& r);
void Portal_Step(int32_t dt_ms, int64_t now_ms);
PortalStats Portal_GetStats();
} // namespace simcore
