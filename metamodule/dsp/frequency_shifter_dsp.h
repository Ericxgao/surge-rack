// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct FrequencyShifter;
// Five engine controls, Extend Frequency, Polyphony, Clock Format.
inline constexpr std::array<float, 8> frequency_shifter_defaults{.5f, 1, 0, 0, 1, 0, 0, 0};
inline constexpr std::array<float, 5> frequency_shifter_minimum{-10, -1, -8, 0, 0},
    frequency_shifter_range{20, 2, 8, 1, 1};
inline constexpr std::array<const char *, 5> frequency_shifter_names{
    "Left Shift", "Right Multiplier", "Delay Time", "Feedback", "Mix"};
FrequencyShifter *create_frequency_shifter();
void destroy_frequency_shifter(FrequencyShifter *);
void reset_frequency_shifter(FrequencyShifter *);
void frequency_shifter_rack_clock(FrequencyShifter *, bool enabled);
void frequency_shifter_samplerate(FrequencyShifter *, float);
void process_frequency_shifter(FrequencyShifter *, const std::array<float, 8> &,
                               const std::array<PolyValues, 5> &, const StereoPoly &, unsigned,
                               float, bool, StereoPoly &);
} // namespace surge_mm
