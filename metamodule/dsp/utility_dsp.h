// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace surge_mm
{
constexpr unsigned utility_poly = 4;
using PolyValues = std::array<float, utility_poly>;
using StereoPoly = std::array<PolyValues, 2>;
struct MixerNoise
{
    float history[utility_poly][2][2]{};
    uint32_t random_state{0x2468ace1};
    float next(unsigned voice, unsigned side, float color);
};
// Native adaptation of src/Mixer.h and DecibelParamQuantity in src/XTModule.h.
// Inputs are Rack volts; gain controls retain the Rack cubic amplitude curve.
void mix_sample(MixerNoise &, const std::array<PolyValues, 6> &audio,
                const std::array<PolyValues, 8> &controls, const std::array<bool, 6> &routes,
                unsigned channels, StereoPoly &output);
struct DigitalRing;
DigitalRing *create_digital_ring();
void destroy_digital_ring(DigitalRing *);
void reset_digital_ring(DigitalRing *);
// Two stereo combinators, 13 original CXOR algorithms, 2x half-band filtering.
void process_digital_ring(DigitalRing *, const std::array<PolyValues, 8> &audio,
                          const std::array<PolyValues, 3> &controls,
                          std::array<unsigned, 2> channels, std::array<PolyValues, 4> &output);
} // namespace surge_mm
