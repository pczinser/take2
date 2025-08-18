#include <dmsdk/sdk.h>
#include "sim_time.hpp"

namespace simcore {

    void TimeInit() {
        detail::s_prev = NowSeconds();
        detail::s_accum = 0.0;
    }

    double NowSeconds() {
        // dmTime::GetTime() returns microseconds
        return dmTime::GetTime() * 1.0e-6;
    }

} // namespace simcore
