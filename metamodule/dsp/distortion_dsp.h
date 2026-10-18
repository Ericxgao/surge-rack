// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Distortion;
// Original twelve controls; pre/post cut enabled, polyphony, three extended ranges.
inline constexpr std::array<float, 18> distortion_defaults{
    .5f, 60.f / 130, .4f, 1, .5f, .5f, .5f, 60.f / 130, .4f, 1, .5f, 0, 1, 1, 0, 0, 0, 0};
inline constexpr std::array<const char *, 12> distortion_names{
    "Pre Gain",  "Pre Frequency",  "Pre Bandwidth",  "Pre High Cut",  "Drive",       "Feedback",
    "Post Gain", "Post Frequency", "Post Bandwidth", "Post High Cut", "Output Gain", "Model"};
inline constexpr std::array<float, 12> distortion_minimum{-48, -60, 0, -60, -24, -1,
                                                          -48, -60, 0, -60, -24, 0};
inline constexpr std::array<float, 12> distortion_range{96, 130, 5, 130, 48, 2,
                                                        96, 130, 5, 130, 48, 7};
inline constexpr float distortion_cv_scale(unsigned i)
{
    return i == 1 || i == 3 || i == 7 || i == 9 ? 12.f / 130 : .2f;
}
Distortion *create_distortion();
void destroy_distortion(Distortion *);
void reset_distortion(Distortion *);
void distortion_samplerate(Distortion *, float);
void distortion_rack_mode(Distortion *, bool);
void process_distortion(Distortion *, const std::array<float, 18> &,
                        const std::array<PolyValues, 12> &, const StereoPoly &, unsigned channels,
                        StereoPoly &);
} // namespace surge_mm
