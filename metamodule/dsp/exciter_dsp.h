// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
#include <cmath>
namespace surge_mm
{
struct Exciter;
inline constexpr std::array<float, 6> exciter_defaults{.5f, .5f, .5f, .5f, .5f, 0};
inline constexpr std::array<const char *, 5> exciter_names{"Drive", "Tone", "Attack", "Release",
                                                           "Mix"};
inline const float exciter_time_min = std::log2(5.f) / std::log2(100.f);
inline const float exciter_time_range = std::log2(20.f) / std::log2(100.f) - exciter_time_min;
inline constexpr float exciter_input_scale = .666666f;
Exciter *create_exciter();
void destroy_exciter(Exciter *);
void reset_exciter(Exciter *);
void exciter_rack_mode(Exciter *, bool);
void exciter_samplerate(Exciter *, float);
void process_exciter(Exciter *, const std::array<float, 6> &, const std::array<PolyValues, 5> &,
                     const StereoPoly &, unsigned channels, StereoPoly &);
} // namespace surge_mm
