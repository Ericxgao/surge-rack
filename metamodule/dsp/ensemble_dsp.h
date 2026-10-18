// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
#include <cmath>
namespace surge_mm
{
struct Ensemble;
// Twelve original engine controls, Reconstruction Filter On, Polyphony.
inline const std::array<float, 12> ensemble_minimum{
    -60, std::log2(.01f), 0, std::log2(.01f), 0, 0, 1.5f, 0, 0, -1, 0, -60};
inline const std::array<float, 12> ensemble_range{130,   std::log2(20.f) - std::log2(.01f),
                                                  1,     std::log2(20.f) - std::log2(.01f),
                                                  1,     6,
                                                  98.5f, 1,
                                                  1,     2,
                                                  1,     130};
inline const std::array<float, 14> ensemble_defaults{
    (45.2326448f + 60) / 130,
    (std::log2(.18f) - ensemble_minimum[1]) / ensemble_range[1],
    1,
    (std::log2(5.52f) - ensemble_minimum[3]) / ensemble_range[3],
    .6f,
    2.f / 6,
    38.5f / 98.5f,
    0,
    0,
    1,
    1,
    (45.2326448f + 60) / 130,
    1,
    0};
inline constexpr std::array<const char *, 12> ensemble_names{
    "Input Filter", "LFO Rate 1", "Depth 1",  "LFO Rate 2", "Depth 2", "Type",
    "BBD Clock",    "Saturation", "Feedback", "Width",      "Mix",     "Output Filter"};
Ensemble *create_ensemble();
void destroy_ensemble(Ensemble *);
void reset_ensemble(Ensemble *);
void ensemble_rack_mode(Ensemble *, bool);
void ensemble_samplerate(Ensemble *, float);
void process_ensemble(Ensemble *, const std::array<float, 14> &, const std::array<PolyValues, 12> &,
                      const StereoPoly &, unsigned channels, StereoPoly &);
} // namespace surge_mm
