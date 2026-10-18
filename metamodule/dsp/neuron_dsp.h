// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Neuron;
// Original twelve controls, LFO Active, Polyphony, Clock Format.
inline constexpr std::array<float, 15> neuron_defaults{
    .5f, .5f, .5f, 1, 0, 1, 96.5f / 192, 0, 5.f / 16, 0, .5f, .5f, 1, 0, 0};
inline constexpr std::array<const char *, 12> neuron_names{
    "Drive",   "Squash",   "Stability", "Asymmetry", "Bias",  "Comb Frequency",
    "Spacing", "Waveform", "Rate",      "Depth",     "Width", "Gain"};
inline constexpr std::array<float, 12> neuron_minimum{-24, 0, 0,  0, 0,   -60,
                                                      -96, 0, -7, 0, -24, -24};
inline constexpr std::array<float, 12> neuron_range{48, 1, 1, 1, 1, 130, 192, 5, 16, 1, 48, 48};
inline constexpr float neuron_cv_scale(unsigned i)
{
    return i == 5 ? 12.f / 130 : i == 6 ? 12.f / 192 : .2f;
}
Neuron *create_neuron();
void destroy_neuron(Neuron *);
void reset_neuron(Neuron *);
void neuron_rack_clock(Neuron *, bool enabled);
void neuron_samplerate(Neuron *, float);
void process_neuron(Neuron *, const std::array<float, 15> &, const std::array<PolyValues, 12> &,
                    const StereoPoly &, unsigned channels, float clock, bool clock_connected,
                    StereoPoly &);
} // namespace surge_mm
