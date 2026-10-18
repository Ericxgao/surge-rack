// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct TreeMonster;
// Eight engine controls, low/high cut enables, polyphony (on as in Rack).
inline constexpr std::array<float, 11> treemonster_defaults{.75f, .5f, 0, 1, .5f, .5f,
                                                            1,    1,   1, 1, 1};
inline constexpr std::array<float, 8> treemonster_minimum{-96, 0, -60, -60, -60, 0, -1, 0},
    treemonster_range{96, 1, 130, 130, 120, 1, 2, 1};
inline constexpr std::array<const char *, 8> treemonster_names{
    "Threshold", "Speed", "Low Cut", "High Cut", "Pitch", "Ring Modulation", "Width", "Mix"};
TreeMonster *create_treemonster();
void destroy_treemonster(TreeMonster *);
void reset_treemonster(TreeMonster *);
void treemonster_samplerate(TreeMonster *, float);
void treemonster_rack_mode(TreeMonster *, bool);
void process_treemonster(TreeMonster *, const std::array<float, 11> &,
                         const std::array<PolyValues, 8> &, const StereoPoly &, unsigned,
                         std::array<PolyValues, 4> &);
} // namespace surge_mm
