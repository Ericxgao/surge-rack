// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
inline constexpr std::array<float, 12> tuned_plus_defaults{.5, .5, .5, .98, 1, 0,
                                                           1,  0,  0,  .05, 0, .5};
inline constexpr std::array<unsigned, 7> tuned_plus_cv_params{1, 2, 3, 4, 5, 6, 9};
inline constexpr std::array<float, 7> tuned_plus_minimum{-5, -100, -100, 0, -80, -110, 0};
inline constexpr std::array<float, 7> tuned_plus_range{10, 200, 200, 1, 110, 110, 1};
struct TunedDelayPlus;
TunedDelayPlus *create_tuned_delay_plus();
void destroy_tuned_delay_plus(TunedDelayPlus *);
void reset_tuned_delay_plus(TunedDelayPlus *);
void tuned_delay_plus_samplerate(TunedDelayPlus *, float);
void process_tuned_delay_plus(TunedDelayPlus *, const std::array<float, 12> &,
                              const std::array<PolyValues, 7> &cv, const StereoPoly &audio,
                              const StereoPoly &feedback, const PolyValues &pitch,
                              const PolyValues &excite, unsigned channels, StereoPoly &output);
} // namespace surge_mm
