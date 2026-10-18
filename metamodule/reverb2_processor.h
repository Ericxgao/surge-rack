// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "reverb2_dsp.h"
class SurgeReverb2 : public UtilityProcessor<12, 13, 2>
{
    std::unique_ptr<surge_mm::Reverb2, decltype(&surge_mm::destroy_reverb2)> engine{
        surge_mm::create_reverb2(), surge_mm::destroy_reverb2};
    bool rack_history{};

  public:
    SurgeReverb2()
    {
        params = surge_mm::reverb2_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::reverb2_samplerate(engine.get(), sr);
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
            params[10] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[10] > .5f)
        {
            for (unsigned s = 0; s < 2; ++s)
                for (unsigned c = 0; c < channels; ++c)
                    audio[s][c] = input(s, c, false);
        }
        else
            for (unsigned s = 0; s < 2; ++s)
                for (unsigned c = 0; c < 4; ++c)
                    audio[s][0] += input(s, c, false);
        if (input_channels[0] && !input_channels[1])
            audio[1] = audio[0];
        std::array<surge_mm::PolyValues, 10> cv{};
        for (unsigned i = 0; i < 10; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        surge_mm::process_reverb2(engine.get(), params, cv, audio, channels, input(12, 0),
                                  input_channels[12] > 0, outputs);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_history ? "surge-fx-history-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-fx-history-rack-v1")
        {
            rack_history = true;
            surge_mm::reverb2_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_history = false;
            surge_mm::reverb2_rack_mode(engine.get(), false);
            params = surge_mm::reverb2_defaults;
            surge_mm::reset_reverb2(engine.get());
        }
    }
};
