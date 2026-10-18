// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
#include <cmath>
namespace surge_mm
{
struct Vocoder;
// Nine continuous controls, Bands (4/8/12/16/20), Input (sum/left/right/stereo), Polyphony.
inline const std::array<float, 12> vocoder_defaults{
    .5f,
    0,
    0,
    .5f,
    (12.f * std::log(180.f / 440.f) / std::log(2.f) + 36) / 72,
    12.f * std::log(7500.f / 440.f) / std::log(2.f) / 60,
    .5f,
    .5f,
    1,
    1,
    0,
    0};
inline constexpr std::array<float, 9> vocoder_minimum{-48, -96, 0, -1, -36, 0, -1, -1, 0},
    vocoder_range{96, 96, 1, 2, 72, 60, 2, 2, 1};
inline constexpr std::array<unsigned, 9> vocoder_engine_ids{0, 1, 2, 3, 6, 7, 9, 10, 11};
inline constexpr std::array<const char *, 9> vocoder_names{"Modulator Gain",
                                                           "Gate",
                                                           "Envelope Follow",
                                                           "Resonance",
                                                           "Min Frequency",
                                                           "Max Frequency",
                                                           "Modulator Range",
                                                           "Modulator Center",
                                                           "Mix"};
Vocoder *create_vocoder();
void destroy_vocoder(Vocoder *);
void reset_vocoder(Vocoder *);
void vocoder_samplerate(Vocoder *, float);
void vocoder_rack_mode(Vocoder *, bool);
void process_vocoder(Vocoder *, const std::array<float, 12> &, const std::array<PolyValues, 9> &,
                     const StereoPoly &, const StereoPoly &, unsigned, StereoPoly &);
} // namespace surge_mm
