// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct EGxVCA;
EGxVCA *create_eg_vca();
void destroy_eg_vca(EGxVCA *);
void set_eg_vca_samplerate(EGxVCA *, float);
void eg_vca_rack_mode(EGxVCA *, bool);
void reset_eg_vca(EGxVCA *);
void process_eg_vca(EGxVCA *, const std::array<float, 17> &, const std::array<PolyValues, 11> &,
                    const std::array<unsigned, 4> &, std::array<PolyValues, 4> &, unsigned &,
                    bool rack_compat = false);
} // namespace surge_mm
