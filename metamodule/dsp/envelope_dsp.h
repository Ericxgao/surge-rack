// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
constexpr float ad_time_span = 11.32192809489f;
struct QuadAD;
QuadAD *create_quad_ad();
void destroy_quad_ad(QuadAD *);
void set_quad_ad_samplerate(QuadAD *, float);
void reset_quad_ad(QuadAD *);
void process_quad_ad(QuadAD *, const std::array<float, 33> &, const std::array<PolyValues, 12> &,
                     const std::array<unsigned, 4> &, std::array<PolyValues, 4> &,
                     std::array<unsigned, 4> &, bool rack_compat = false);
} // namespace surge_mm
