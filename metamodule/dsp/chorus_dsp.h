// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Chorus;
// Stable native parameter IDs; mirrored by panel metadata and Hub conversion.
namespace chorus_param
{
inline constexpr unsigned time = 0, rate = 1, depth = 2, feedback = 3;
inline constexpr unsigned low_cut = 4, high_cut = 5, mix = 6, width = 7;
inline constexpr unsigned low_cut_on = 8, high_cut_on = 9, polyphony = 10, clock_format = 11;
} // namespace chorus_param
namespace chorus_input
{
inline constexpr unsigned first_cv = 2, clock = 10;
} // namespace chorus_input

// Eight original controls, Low Cut On, High Cut On, Polyphony, Clock Format.
inline constexpr std::array<float, 12> chorus_defaults{
    5.f / 8, 5.f / 16, .3f, .5f, 24.f / 130, 96.f / 130, 1, .5f, 1, 1, 0, 0};
inline constexpr std::array<const char *, 8> chorus_names{
    "Time", "Rate", "Depth", "Feedback", "Low Cut", "High Cut", "Mix", "Width"};
inline constexpr std::array<float, 8> chorus_minimum{-11, -7, 0, 0, -60, -60, 0, -24};
inline constexpr std::array<float, 8> chorus_range{8, 16, 1, 1, 130, 130, 1, 48};
Chorus *create_chorus();
void destroy_chorus(Chorus *);
void reset_chorus(Chorus *);
void chorus_rack_clock(Chorus *, bool enabled);
void chorus_samplerate(Chorus *, float);
void process_chorus(Chorus *, const std::array<float, 12> &, const std::array<PolyValues, 8> &,
                    const StereoPoly &, unsigned channels, float clock, bool clock_connected,
                    StereoPoly &);
} // namespace surge_mm
