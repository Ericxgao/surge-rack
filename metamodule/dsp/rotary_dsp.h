// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Rotary;
// Original eight parameters, Drive Enabled, Polyphony, Clock Format.
inline constexpr std::array<float, 11> rotary_defaults{.5f,       .25f, .5f, .35f, 0, 0,
                                                       25.f / 48, 1,    1,   0,    0};
inline constexpr std::array<const char *, 8> rotary_names{
    "Horn Rate", "Doppler", "Tremolo", "Rotor Rate", "Drive", "Model", "Width", "Mix"};
inline constexpr std::array<float, 8> rotary_minimum{-7, 0, 0, 0, 0, 0, -24, 0};
inline constexpr std::array<float, 8> rotary_range{16, 1, 1, 2, 1, 7, 48, 1};
Rotary *create_rotary();
void destroy_rotary(Rotary *);
void reset_rotary(Rotary *);
void rotary_rack_clock(Rotary *, bool enabled);
void rotary_samplerate(Rotary *, float);
void process_rotary(Rotary *, const std::array<float, 11> &, const std::array<PolyValues, 8> &,
                    const StereoPoly &, unsigned channels, float clock, bool clock_connected,
                    StereoPoly &);
} // namespace surge_mm
