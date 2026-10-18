// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "vocoder_dsp.h"
class SurgeVocoder : public UtilityProcessor<12, 13, 2>
{
    std::unique_ptr<surge_mm::Vocoder, decltype(&surge_mm::destroy_vocoder)> engine{
        surge_mm::create_vocoder(), surge_mm::destroy_vocoder};
    bool rack_compat{};

  public:
    SurgeVocoder()
    {
        params = surge_mm::vocoder_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::vocoder_samplerate(engine.get(), sr);
    }
    void update() override
    {
        if (bypassed)
        {
            for (unsigned s = 0; s < 2; ++s)
            {
                output_count(s, std::max(1u, std::min(unsigned(input_channels[s]), 4u)));
                for (unsigned c = 0; c < 4; ++c)
                    outputs[s][c] = input(s, c, false);
            }
            return;
        }
        unsigned channels =
            params[11] > .5f
                ? std::clamp<unsigned>(rack_compat
                                           ? std::max(input_channels[0], input_channels[1])
                                           : std::max({input_channels[0], input_channels[1],
                                                       input_channels[2], input_channels[3]}),
                                       1, 4)
                : 1;
        surge_mm::StereoPoly audio{}, modulator{};
        for (unsigned s = 0; s < 2; ++s)
        {
            if (params[11] > .5f)
            {
                for (unsigned c = 0; c < channels; ++c)
                {
                    audio[s][c] = input(s, c, !rack_compat);
                    if (!rack_compat)
                        modulator[s][c] = input(2 + s, c);
                }
                if (rack_compat)
                    for (unsigned c = 0; c < 4; ++c)
                        modulator[s][0] += input(2 + s, c, false);
            }
            else
                for (unsigned c = 0; c < 4; ++c)
                {
                    audio[s][0] += input(s, c, false);
                    modulator[s][0] += input(2 + s, c, false);
                }
        }
        if (input_channels[0] && !input_channels[1])
            audio[1] = audio[0];
        if (input_channels[2] && !input_channels[3])
            modulator[1] = modulator[0];
        std::array<surge_mm::PolyValues, 9> cv{};
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(4 + i, c);
        surge_mm::process_vocoder(engine.get(), params, cv, audio, modulator, channels, outputs);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-vocoder-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-vocoder-rack-v1")
        {
            rack_compat = true;
            surge_mm::vocoder_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::vocoder_rack_mode(engine.get(), false);
            params = surge_mm::vocoder_defaults;
            surge_mm::reset_vocoder(engine.get());
        }
    }
};
