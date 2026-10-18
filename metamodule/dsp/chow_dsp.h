// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Chow;
inline constexpr std::array<float, 5> chow_defaults{.75f, 9.f / 19, 0, 1, 0};
inline constexpr std::array<const char *, 4> chow_names{"Threshold", "Ratio", "Flip", "Mix"};
inline constexpr std::array<float, 4> chow_minimum{-96, 1, 0, 0};
inline constexpr std::array<float, 4> chow_range{96, 19, 1, 1};
Chow *create_chow();
void destroy_chow(Chow *);
void reset_chow(Chow *);
void chow_rack_mode(Chow *, bool);
void chow_samplerate(Chow *, float);
void process_chow(Chow *, const std::array<float, 5> &, const std::array<PolyValues, 4> &,
                  const StereoPoly &, unsigned channels, StereoPoly &);
} // namespace surge_mm
