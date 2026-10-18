// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
inline constexpr unsigned lfo_parameter_count = 58;
inline constexpr std::array<float, 10> lfo_minimum{-7, 0, -1, 0, -8, -8, -8, -8, 0, -8};
inline constexpr std::array<float, 10> lfo_range{16, 1, 2, 1, 13, 13, 13, 13, 1, 13};
inline constexpr auto lfo_defaults = []() {
    std::array<float, lfo_parameter_count> p{};
    p[0] = 7.f / 16;
    p[2] = .5;
    p[3] = 1;
    p[6] = p[7] = 8.f / 13;
    p[8] = p[9] = 1;
    p[13] = 1.f / 3;
    for (unsigned i = 0; i < 16; ++i)
        p[17 + i] = .5f + .5f * float(i) / 15;
    p[50] = p[51] = 1;
    p[57] = .75;
    return p;
}();
struct LFO;
LFO *create_lfo();
void destroy_lfo(LFO *);
void lfo_rack_mode(LFO *, bool);
void reset_lfo(LFO *);
void lfo_samplerate(LFO *, float);
void process_lfo(LFO *, std::array<float, lfo_parameter_count> &,
                 const std::array<PolyValues, 14> &, const std::array<uint8_t, 14> &,
                 unsigned channels, std::array<PolyValues, 6> &, bool rack_compat = false);
} // namespace surge_mm
