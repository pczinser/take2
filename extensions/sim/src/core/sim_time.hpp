#pragma once
#include <cstdint>
#include <cmath>

namespace simcore {

// Initialize the clock (call once during extension Initialize()).
void   TimeInit();      // should set a monotonic "start" reference
double NowSeconds();    // must be monotonic; do NOT use wall clock

// -----------------------------------------------------------------------------
// 1) Header-only fixed-step driver, returns leftover alpha so the caller can interpolate.
// -----------------------------------------------------------------------------
namespace detail {
    inline double s_prev      = 0.0;    // last NowSeconds()
    inline double s_accum     = 0.0;    // accumulated time
    inline double s_max_accum = 0.25;   // clamp to avoid spiral-of-death
}

template <class Fn>
inline double FixedStep(Fn&& step, double dt_fixed) {
    double now = NowSeconds();
    double frame_dt = now - detail::s_prev;
    detail::s_prev = now;
    if (frame_dt > detail::s_max_accum) frame_dt = detail::s_max_accum;

    detail::s_accum += frame_dt;
    int steps = 0;
    while (detail::s_accum >= dt_fixed) {
        step((float)dt_fixed);
        detail::s_accum -= dt_fixed;
        if (++steps > 8) {           // safety cap
            detail::s_accum = std::fmod(detail::s_accum, dt_fixed);
            break;
        }
    }
    // Return leftover fraction for render interpolation
    return detail::s_accum / dt_fixed; // alpha in [0,1)
}

// -----------------------------------------------------------------------------
// 2) Re-entrant stepper. No shared globals; multiple loops OK.
// -----------------------------------------------------------------------------
struct FixedStepper {
    double prev      = 0.0;
    double accum     = 0.0;
    double max_accum = 0.25;

    void init(double now_sec) {
        prev  = now_sec;
        accum = 0.0;
    }

    template <class Fn>
    double step(Fn&& fn, double dt_fixed) {
        double now = NowSeconds();
        double frame_dt = now - prev;
        prev = now;
        if (frame_dt > max_accum) frame_dt = max_accum;

        accum += frame_dt;
        int steps = 0;
        while (accum >= dt_fixed) {
            fn((float)dt_fixed);
            accum -= dt_fixed;
            if (++steps > 8) { accum = std::fmod(accum, dt_fixed); break; }
        }
        return accum / dt_fixed; // alpha
    }
};

} // namespace simcore
