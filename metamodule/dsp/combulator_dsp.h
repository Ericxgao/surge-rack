// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Combulator;
inline constexpr float combulator_low = -117.3763f, combulator_high = 70.f;
inline constexpr std::array<float, 16> combulator_defaults{0,
                                                           (-9 - combulator_low) /
                                                               (combulator_high - combulator_low),
                                                           60.25f / 120,
                                                           59.75f / 120,
                                                           .875f,
                                                           .5f,
                                                           .7937005f,
                                                           .7937005f,
                                                           .7937005f,
                                                           .625f,
                                                           .375f,
                                                           1,
                                                           1,
                                                           0,
                                                           0,
                                                           0};
inline constexpr std::array<const char *, 12> combulator_names{
    "Extra Noise", "Center Frequency", "Comb 2 Pitch", "Comb 3 Pitch", "Feedback", "Tone",
    "Comb 1 Gain", "Comb 2 Gain",      "Comb 3 Gain",  "Pan 2",        "Pan 3",    "Mix"};
Combulator *create_combulator();
void destroy_combulator(Combulator *);
void reset_combulator(Combulator *);
void combulator_samplerate(Combulator *, float);
void combulator_rack_mode(Combulator *, bool);
void process_combulator(Combulator *, const std::array<float, 16> &,
                        const std::array<PolyValues, 12> &, const StereoPoly &, unsigned,
                        StereoPoly &);
} // namespace surge_mm
