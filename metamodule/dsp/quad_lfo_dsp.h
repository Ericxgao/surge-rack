// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct QuadLFO;
QuadLFO *create_quad_lfo();
void destroy_quad_lfo(QuadLFO *);
void set_quad_lfo_samplerate(QuadLFO *, float);
void quad_lfo_rack_mode(QuadLFO *, bool);
void reset_quad_lfo(QuadLFO *);
void process_quad_lfo(QuadLFO *, const std::array<float, 19> &, const std::array<PolyValues, 12> &,
                      const std::array<unsigned, 4> &, std::array<PolyValues, 4> &,
                      std::array<unsigned, 4> &, bool rack_compat = false);
} // namespace surge_mm
