// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Flanger;
// Original eleven parameters, then Polyphony and Clock Format.
inline constexpr std::array<float, 13> flanger_defaults{0, 0,   5.f / 16, 1,   1, 60.f / 127, 0,
                                                        0, .1f, .5f,      .9f, 0, 0};
inline constexpr std::array<const char *, 11> flanger_names{
    "Mode",    "Waveform", "Rate",    "Depth", "Count", "Pitch",
    "Spacing", "Feedback", "Damping", "Width", "Mix"};
Flanger *create_flanger();
void destroy_flanger(Flanger *);
void reset_flanger(Flanger *);
void flanger_rack_clock(Flanger *, bool enabled);
void flanger_samplerate(Flanger *, float);
void process_flanger(Flanger *, const std::array<float, 13> &, const std::array<PolyValues, 11> &,
                     const StereoPoly &, unsigned channels, float clock, bool clock_connected,
                     StereoPoly &);
} // namespace surge_mm
