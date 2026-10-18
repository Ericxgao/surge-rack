// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
#include <algorithm>
namespace surge_mm
{
struct UnisonMap
{
    unsigned channels{1}, voices{3}, banks{};
    bool fits{};
    std::array<unsigned, 4> bank_channels{};
    std::array<std::array<int, 4>, 4> source{}, voice{};
    void configure(unsigned input_channels, unsigned unison_voices)
    {
        channels = std::clamp(input_channels, 1u, 4u);
        voices = std::clamp(unison_voices, 1u, 9u);
        fits = channels * voices <= 16;
        banks = fits ? (channels * voices + 3) / 4 : 0;
        bank_channels = {};
        for (auto &bank : source)
            bank.fill(-1);
        for (auto &bank : voice)
            bank.fill(-1);
        if (!fits)
            return;
        for (unsigned c = 0; c < channels; ++c)
            for (unsigned v = 0; v < voices; ++v)
            {
                unsigned slot = c * voices + v, bank = slot / 4, lane = slot % 4;
                source[bank][lane] = c;
                voice[bank][lane] = v;
                bank_channels[bank] = lane + 1;
            }
    }
};
struct Unison;
Unison *create_unison();
void destroy_unison(Unison *);
void reset_unison(Unison *);
void set_unison_samplerate(Unison *, float);
void process_unison(Unison *, const std::array<float, 9> &, const std::array<PolyValues, 9> &,
                    const UnisonMap &, bool stereo, bool audio_ready, std::array<PolyValues, 6> &,
                    bool rack_compat = false);
// Expander mapping is identical for each of its two CV sources.
inline std::array<PolyValues, 4> expand_unison_cv(const UnisonMap &map, const PolyValues &input,
                                                  unsigned channels)
{
    std::array<PolyValues, 4> result{};
    if (!channels || !map.fits)
        return result;
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned lane = 0; lane < map.bank_channels[bank]; ++lane)
        {
            unsigned source = channels == 1 ? 0 : unsigned(map.source[bank][lane]);
            if (source < std::min(channels, 4u))
                result[bank][lane] = input[source];
        }
    return result;
}
} // namespace surge_mm
