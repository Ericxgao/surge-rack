// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Bonsai;
// Eleven engine controls, Polyphony, Bass Routing, Bass Range.
inline constexpr std::array<float, 14> bonsai_defaults{.5f,  .5f, .25f, 0, 0, .25f, .25f,
                                                       .25f, 0,   .5f,  1, 0, 0,    0};
inline constexpr std::array<const char *, 11> bonsai_names{
    "Input Gain",        "Bass Amount", "Bass Distort", "Bias Filter", "Distortion", "Saturation",
    "Noise Sensitivity", "Noise Gain",  "Dull",         "Output Gain", "Mix"};
Bonsai *create_bonsai();
void destroy_bonsai(Bonsai *);
void reset_bonsai(Bonsai *);
void bonsai_samplerate(Bonsai *, float);
void bonsai_rack_mode(Bonsai *, bool);
void process_bonsai(Bonsai *, const std::array<float, 14> &, const std::array<PolyValues, 11> &,
                    const StereoPoly &, unsigned, StereoPoly &);
} // namespace surge_mm
