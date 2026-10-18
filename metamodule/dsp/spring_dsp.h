// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct SpringReverb;
inline constexpr std::array<float, 9> spring_defaults{.5f, .5f, 1, .5f, .5f, 0, 0, .5f, 0};
inline constexpr std::array<unsigned, 7> spring_cv_ids{0, 1, 2, 3, 4, 5, 7};
inline constexpr std::array<const char *, 8> spring_names{
    "Size", "Decay", "Reflections", "HF Damping", "Spin", "Chaos", "Knock", "Mix"};
SpringReverb *create_spring();
void destroy_spring(SpringReverb *);
void reset_spring(SpringReverb *);
void spring_samplerate(SpringReverb *, float);
// Prepare the separate imported Sum engine while loading state, outside update().
void spring_rack_mode(SpringReverb *, bool);
void process_spring(SpringReverb *, const std::array<float, 9> &, const std::array<PolyValues, 8> &,
                    const std::array<bool, 4> &, const StereoPoly &, unsigned, StereoPoly &,
                    const PolyValues &gate = {}, bool rack_trigger = false);
} // namespace surge_mm
