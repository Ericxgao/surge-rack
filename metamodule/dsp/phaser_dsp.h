// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct Phaser;
// Original SST parameter order, then Tone Enabled, LFO Enabled, Polyphony, Clock Format.
inline constexpr std::array<float, 16> phaser_defaults{.5f,      .5f, .5f, 7.f / 16, 0, 0, .5f, .5f,
                                                       3.f / 15, 0,   0,   .5f,      1, 1, 0,   0};
inline constexpr std::array<const char *, 12> phaser_names{
    "Center", "Feedback", "Sharpness", "Rate",   "Depth",    "Stereo",
    "Mix",    "Width",    "Stages",    "Spread", "Waveform", "Tone"};
Phaser *create_phaser();
void destroy_phaser(Phaser *);
void reset_phaser(Phaser *);
void phaser_rack_clock(Phaser *, bool enabled);
void phaser_samplerate(Phaser *, float);
void process_phaser(Phaser *, const std::array<float, 16> &, const std::array<PolyValues, 12> &,
                    const StereoPoly &, unsigned channels, float clock, bool clock_connected,
                    StereoPoly &);
} // namespace surge_mm
