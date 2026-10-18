// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Reverb2;
inline constexpr std::array<float, 12> reverb2_defaults{4.f / 9, .5f, .475f, 1,    1, .5f,
                                                        .2f,     .2f, .5f,   .33f, 0, 0};
inline constexpr std::array<float, 10> reverb2_minimum{-8, -1, -4, 0, 0, 0, 0, 0, -24, 0};
inline constexpr std::array<float, 10> reverb2_range{9, 2, 10, 1, 1, 1, 1, 1, 48, 1};
inline constexpr std::array<const char *, 10> reverb2_names{
    "Pre-Delay",  "Size",       "Decay",      "Diffusion", "Buildup",
    "Modulation", "LF Damping", "HF Damping", "Width",     "Mix"};
Reverb2 *create_reverb2();
void destroy_reverb2(Reverb2 *);
void reset_reverb2(Reverb2 *);
void reverb2_samplerate(Reverb2 *, float);
void reverb2_rack_mode(Reverb2 *, bool);
void process_reverb2(Reverb2 *, const std::array<float, 12> &, const std::array<PolyValues, 10> &,
                     const StereoPoly &, unsigned, float, bool, StereoPoly &);
} // namespace surge_mm
