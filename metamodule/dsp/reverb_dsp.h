// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Reverb;
// Shape is a four-position option (0..3); other engine controls are normalized.
inline constexpr std::array<float, 15> reverb_defaults{
    4.f / 13, 0, .5f, .5f, .2f, 36.f / 130, 60.f / 130, .5f, 1, .5f, .5f, 1, 1, 0, 0};
inline constexpr std::array<float, 11> reverb_minimum{-8, 0, 0, -4, 0, -60, -60, -48, -60, 0, -24};
inline constexpr std::array<float, 11> reverb_range{13, 3, 1, 10, 1, 130, 130, 96, 130, 1, 48};
inline constexpr std::array<const char *, 11> reverb_names{
    "Pre-Delay",      "Room Shape", "Size",     "Decay", "Damping", "Low Cut",
    "Peak Frequency", "Peak Gain",  "High Cut", "Mix",   "Width"};
Reverb *create_reverb();
void destroy_reverb(Reverb *);
void reset_reverb(Reverb *);
void reverb_samplerate(Reverb *, float);
void reverb_rack_mode(Reverb *, bool);
void process_reverb(Reverb *, const std::array<float, 15> &, const std::array<PolyValues, 11> &,
                    const StereoPoly &, unsigned, float, bool, StereoPoly &);
} // namespace surge_mm
