// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Resonator;
inline constexpr std::array<float, 3> resonator_low{-34.4936f, -6.6305f, 21.23265f},
    resonator_high{-6.6305f, 21.23265f, 49.09578f};
inline constexpr std::array<float, 19> resonator_defaults{
    (-18.6305f + 34.4936f) / (-6.6305f + 34.4936f),
    .75f,
    .7937005f,
    (8.038216f + 6.6305f) / (21.23265f + 6.6305f),
    .75f,
    .7937005f,
    (35.90135f - 21.23265f) / (49.09578f - 21.23265f),
    .75f,
    .7937005f,
    1.f / 3,
    .5f,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0};
inline constexpr std::array<unsigned, 11> resonator_cv_ids{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11};
inline constexpr std::array<const char *, 12> resonator_names{
    "Band 1 Frequency", "Band 1 Resonance",
    "Band 1 Gain",      "Band 2 Frequency",
    "Band 2 Resonance", "Band 2 Gain",
    "Band 3 Frequency", "Band 3 Resonance",
    "Band 3 Gain",      "Mode",
    "Output",           "Mix"};
Resonator *create_resonator();
void destroy_resonator(Resonator *);
void reset_resonator(Resonator *);
void resonator_samplerate(Resonator *, float);
void process_resonator(Resonator *, const std::array<float, 19> &,
                       const std::array<PolyValues, 11> &, const StereoPoly &, unsigned,
                       StereoPoly &);
} // namespace surge_mm
