// SPDX-License-Identifier: GPL-3.0-or-later
// Native adaptation of Surge Rack Mixer.h and DigitalRingMod.h, copyright
// 2019-2024 Surge Synth Team contributors. Original Surge/SST kernels retained.
#include "utility_dsp.h"
#include "globals.h"
#include <algorithm>
#include <cmath>
#include "CXOR.h"
#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/filters/HalfRateFilter.h"
namespace surge_mm
{
float MixerNoise::next(unsigned voice, unsigned side, float color)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    const float random = (random_state >> 8) * (2.f / 16777216.f) - 1.f;
    return sst::basic_blocks::dsp::correlated_noise_o2mk2_supplied_value(
        history[voice][side][0], history[voice][side][1], color, random);
}
void mix_sample(MixerNoise &noise, const std::array<PolyValues, 6> &audio,
                const std::array<PolyValues, 8> &controls, const std::array<bool, 6> &routes,
                unsigned channels, StereoPoly &output)
{
    for (auto &side : output)
        side.fill(0);
    auto gain = [](float v) {
        v = std::max(v, 0.f);
        return v * v * v;
    };
    for (unsigned voice = 0; voice < channels; ++voice)
    {
        float levels[8];
        for (unsigned p = 0; p < 8; ++p)
            levels[p] = gain(controls[p][voice]);
        for (unsigned side = 0; side < 2; ++side)
        {
            float osc[3];
            for (unsigned i = 0; i < 3; ++i)
                osc[i] = audio[i * 2 + side][voice] * .2f;
            float sum = 0;
            for (unsigned i = 0; i < 3; ++i)
                if (routes[i])
                    sum += osc[i] * levels[i];
            if (routes[3])
                sum +=
                    noise.next(voice, side, std::clamp(controls[6][voice], -1.f, 1.f)) * levels[3];
            if (routes[4])
                sum += osc[0] * osc[1] * levels[4];
            if (routes[5])
                sum += osc[1] * osc[2] * levels[5];
            output[side][voice] = sum * levels[7] * 5.f;
        }
    }
}
struct DigitalRing
{
    static constexpr unsigned block = 8, oversampled = 16;
    struct Voice
    {
        sst::filters::HalfRate::HalfRateFilter up_a{6, true}, up_b{6, true}, down{6, true};
        alignas(16) float a[2][block]{}, b[2][block]{}, out[2][block]{};
        void reset()
        {
            up_a.reset();
            up_b.reset();
            down.reset();
            for (auto &side : a)
                std::fill_n(side, block, 0.f);
            for (auto &side : b)
                std::fill_n(side, block, 0.f);
            for (auto &side : out)
                std::fill_n(side, block, 0.f);
        }
    } voices[2][utility_poly];
    unsigned position{};
    std::array<unsigned, 2> previous_channels{};
};
DigitalRing *create_digital_ring()
{
    return new DigitalRing;
}
void destroy_digital_ring(DigitalRing *ring)
{
    delete ring;
}
void reset_digital_ring(DigitalRing *ring)
{
    for (auto &stage : ring->voices)
        for (auto &voice : stage)
            voice.reset();
    ring->position = 0;
    ring->previous_channels = {};
}
void process_digital_ring(DigitalRing *ring, const std::array<PolyValues, 8> &audio,
                          const std::array<PolyValues, 3> &controls,
                          std::array<unsigned, 2> channels, std::array<PolyValues, 4> &output)
{
    using Kernel = void (*)(float *, float *, float *, unsigned);
    // Ordinals match Surge's CombinatorMode, including the two legacy kernels.
    static const Kernel kernels[]{nullptr,        cxor43_0_block,        cxor43_1_block,
                                  cxor43_2_block, cxor43_3_legacy_block, cxor43_4_legacy_block,
                                  cxor43_3_block, cxor43_4_block,        cxor93_0_block,
                                  cxor93_1_block, cxor93_2_block,        cxor93_3_block,
                                  cxor93_4_block};
    for (unsigned stage = 0; stage < 2; ++stage)
    {
        channels[stage] = std::min(channels[stage], utility_poly);
        for (unsigned voice = 0; voice < utility_poly; ++voice)
            if ((voice >= ring->previous_channels[stage]) != (voice >= channels[stage]))
                ring->voices[stage][voice].reset();
    }
    ring->previous_channels = channels;
    if (ring->position == DigitalRing::block)
    {
        for (unsigned stage = 0; stage < 2; ++stage)
            for (unsigned voice = 0; voice < channels[stage]; ++voice)
            {
                auto &v = ring->voices[stage][voice];
                alignas(16) float a[2][16]{}, b[2][16]{}, result[2][16]{};
                const bool linked = stage == 1 && controls[2][voice] > .5f;
                // Use the first output, as the original Rack switch label promises.
                // A missing first-stage voice contributes zero, never stale buffers.
                if (linked)
                    for (unsigned side = 0; side < 2; ++side)
                        for (unsigned s = 0; s < 8; ++s)
                            v.a[side][s] =
                                voice < channels[0] ? ring->voices[0][voice].out[side][s] : 0;
                v.up_a.process_block_U2(v.a[0], v.a[1], a[0], a[1], 16);
                v.up_b.process_block_U2(v.b[0], v.b[1], b[0], b[1], 16);
                const int mode = std::clamp(int(std::round(controls[stage][voice] * 12)), 0, 12);
                for (unsigned side = 0; side < 2; ++side)
                {
                    if (mode == 0)
                        for (unsigned s = 0; s < 16; ++s)
                            result[side][s] = a[side][s] * b[side][s];
                    else
                        kernels[mode](a[side], b[side], result[side], 4);
                }
                v.down.process_block_D2(result[0], result[1], 16, v.out[0], v.out[1]);
            }
        ring->position = 0;
    }
    for (auto &port : output)
        port.fill(0);
    for (unsigned stage = 0; stage < 2; ++stage)
        for (unsigned voice = 0; voice < channels[stage]; ++voice)
        {
            auto &v = ring->voices[stage][voice];
            for (unsigned side = 0; side < 2; ++side)
            {
                v.a[side][ring->position] = audio[stage * 4 + side][voice];
                v.b[side][ring->position] = audio[stage * 4 + 2 + side][voice];
                output[stage * 2 + side][voice] = v.out[side][ring->position];
            }
        }
    ++ring->position;
}
} // namespace surge_mm
