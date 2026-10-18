// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
#include <string>
namespace surge_mm
{
enum class FilterModule
{
    VCF,
    Waveshaper
};
struct Filter;
Filter *create_filter(FilterModule);
void destroy_filter(Filter *);
void reset_filter(Filter *);
void filter_samplerate(Filter *, float);
unsigned filter_model_count(FilterModule);
unsigned filter_subtype_count(unsigned model);
const char *filter_model_name(FilterModule, unsigned model);
std::string filter_subtype_name(unsigned model, unsigned subtype);
std::array<float, 9> filter_defaults(FilterModule);
// Five normalized controls, Model, Subtype (VCF) or Low/High Cut enable (WS), DC block (WS).
// Audio channels are independent on each side, as in Rack; CV mono broadcast is in the wrapper.
void process_filter(Filter *, const std::array<float, 9> &, const std::array<PolyValues, 5> &cv,
                    const StereoPoly &audio, std::array<unsigned, 2> channels, StereoPoly &out);
} // namespace surge_mm
