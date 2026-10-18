// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "frequency_shifter_dsp.h"
class SurgeFrequencyShifter : public UtilityProcessor<8, 8, 2>
{
    std::unique_ptr<surge_mm::FrequencyShifter, decltype(&surge_mm::destroy_frequency_shifter)>
        engine{surge_mm::create_frequency_shifter(), surge_mm::destroy_frequency_shifter};
    bool rack_clock{};

  public:
    SurgeFrequencyShifter()
    {
        params = surge_mm::frequency_shifter_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::frequency_shifter_samplerate(engine.get(), sr);
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
            params[6] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[6] > .5f)
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
        std::array<surge_mm::PolyValues, 5> cv{};
        for (unsigned i = 0; i < 5; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        surge_mm::process_frequency_shifter(engine.get(), params, cv, audio, channels, input(7, 0),
                                            input_channels[7] > 0, outputs);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_clock ? "surge-fx-clock-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-fx-clock-rack-v1")
        {
            rack_clock = true;
            surge_mm::frequency_shifter_rack_clock(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_clock = false;
            surge_mm::frequency_shifter_rack_clock(engine.get(), false);
            params = surge_mm::frequency_shifter_defaults;
            surge_mm::reset_frequency_shifter(engine.get());
        }
    }
};
