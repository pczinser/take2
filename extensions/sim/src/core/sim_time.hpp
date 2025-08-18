#pragma once
#include <cstdint>

namespace simcore {

	// Internal shared state for the fixed-step accumulator.
	// C++17 inline variables => one definition across all TUs.
	namespace detail {
		inline double s_prev = 0.0;
		inline double s_accum = 0.0;
		inline double s_max_accum = 0.25;
	}

	// Initialize the accumulator clock.
	void   TimeInit();
	// Wall clock (seconds).
	double NowSeconds();

	// Header-only fixed-step driver.
	// Calls step(dt_fixed) 0..N times based on an accumulator.
	template <class Fn>
	inline void FixedStep(Fn&& step, double dt_fixed) {
		double now = NowSeconds();
		double frame_dt = now - detail::s_prev;
		detail::s_prev = now;
		if (frame_dt > detail::s_max_accum) frame_dt = detail::s_max_accum;

		detail::s_accum += frame_dt;
		int steps = 0;
		while (detail::s_accum >= dt_fixed) {
			step((float)dt_fixed);
			detail::s_accum -= dt_fixed;
			if (++steps > 8) { // safety cap
				detail::s_accum = 0.0;
				break;
			}
		}
	}

} // namespace simcore