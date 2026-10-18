// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Nimbus;
inline constexpr std::array<float, 13> nimbus_defaults{0, 0, 0, .75f, .5f, .5f, .5f,
                                                       0, 0, 0, 0,    .5f, 0};
inline constexpr std::array<unsigned, 9> nimbus_cv_params{2, 3, 4, 5, 6, 7, 9, 10, 11};
inline constexpr std::array<float, 9> nimbus_minimum{0, -1, -48, -1, -1, 0, 0, 0, 0};
inline constexpr std::array<float, 9> nimbus_range{1, 2, 96, 2, 2, 1, 1, 1, 1};
inline constexpr std::array<const char *, 9> nimbus_names{
    "Position", "Size / Warp", "Pitch", "Density / Diffusion / Smear", "Texture / Filter", "Spread",
    "Feedback", "Reverb",      "Mix"};
Nimbus *create_nimbus();
void destroy_nimbus(Nimbus *);
void reset_nimbus(Nimbus *);
void nimbus_samplerate(Nimbus *, float);
// Allocate the additional Rack Sum engine during state loading, never in update().
void prepare_nimbus_rack(Nimbus *);
void process_nimbus(Nimbus *, const std::array<float, 13> &, const std::array<PolyValues, 10> &,
                    const StereoPoly &, unsigned, const PolyValues &, const PolyValues &,
                    StereoPoly &, bool rack_trigger = false);
} // namespace surge_mm
