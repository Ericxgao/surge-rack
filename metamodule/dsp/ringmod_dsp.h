// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct RingMod;
// Nine engine controls, low/high cut enables, polyphony, detune range/type.
inline constexpr std::array<float, 14> ringmod_defaults{0, 60.f / 127, .2f, 0, .3f, .7f, 0,
                                                        1, 1,          1,   1, 0,   0,   0};
inline constexpr std::array<float, 9> ringmod_minimum{0, 0, 0, 1, 0, 0, -60, -60, 0},
    ringmod_range{28, 127, 1, 15, 1, 1, 130, 130, 1};
inline constexpr std::array<const char *, 9> ringmod_names{
    "Shape",         "Frequency", "Detune",   "Voices", "Forward Bias",
    "Linear Region", "Low Cut",   "High Cut", "Mix"};
RingMod *create_ringmod();
void destroy_ringmod(RingMod *);
void reset_ringmod(RingMod *);
void ringmod_samplerate(RingMod *, float);
void ringmod_rack_mode(RingMod *, bool);
bool process_ringmod(RingMod *, const std::array<float, 14> &, const std::array<PolyValues, 9> &,
                     const StereoPoly &, const StereoPoly &, unsigned, bool, StereoPoly &);
} // namespace surge_mm
