// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "ringmod_dsp.h"
class SurgeRingMod : public UtilityProcessor<14, 13, 2>
{
    std::unique_ptr<surge_mm::RingMod, decltype(&surge_mm::destroy_ringmod)> engine{
        surge_mm::create_ringmod(), surge_mm::destroy_ringmod};
    bool rack_compat{};

  public:
    SurgeRingMod()
    {
        params = surge_mm::ringmod_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::ringmod_samplerate(engine.get(), sr);
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
        bool connected = input_channels[11] || input_channels[12];
        // Match Rack's automatic source selection and return to Sine on unplug.
        if (!rack_compat)
        {
            if (connected)
                params[0] = 1;
            else if (std::round(params[0] * 28) == 28)
                params[0] = 0;
        }
        unsigned channels =
            params[11] > .5f
                ? std::clamp<unsigned>(rack_compat
                                           ? std::max(input_channels[0], input_channels[1])
                                           : std::max({input_channels[0], input_channels[1],
                                                       input_channels[11], input_channels[12]}),
                                       1, 4)
                : 1;
        surge_mm::StereoPoly audio{}, carrier{};
        if (params[11] > .5f)
        {
            for (unsigned s = 0; s < 2; ++s)
                for (unsigned c = 0; c < channels; ++c)
                {
                    audio[s][c] = input(s, c, !rack_compat);
                    if (!rack_compat)
                        carrier[s][c] = input(11 + s, c);
                }
        }
        else
            for (unsigned s = 0; s < 2; ++s)
                for (unsigned c = 0; c < 4; ++c)
                {
                    audio[s][0] += input(s, c, false);
                    carrier[s][0] += input(11 + s, c, false);
                }
        if (rack_compat && params[11] > .5f)
            for (unsigned s = 0; s < 2; ++s)
                for (unsigned c = 0; c < 4; ++c)
                    carrier[s][0] += input(11 + s, c, false);
        if (input_channels[0] && !input_channels[1])
            audio[1] = audio[0];
        if (input_channels[11] && !input_channels[12])
            carrier[1] = carrier[0];
        std::array<surge_mm::PolyValues, 9> cv{};
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        bool completed = surge_mm::process_ringmod(engine.get(), params, cv, audio, carrier,
                                                   channels, connected, outputs);
        if (rack_compat && completed)
        {
            if (connected)
                params[0] = 1;
            else if (std::round(params[0] * 28) == 28)
                params[0] = 0;
        }
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-ringmod-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-ringmod-rack-v1")
        {
            rack_compat = true;
            surge_mm::ringmod_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::ringmod_rack_mode(engine.get(), false);
            params = surge_mm::ringmod_defaults;
            surge_mm::reset_ringmod(engine.get());
        }
    }
};
