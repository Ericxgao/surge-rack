// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "distortion_dsp.h"
class SurgeDistortion : public UtilityProcessor<18, 14, 2>
{
    std::unique_ptr<surge_mm::Distortion, decltype(&surge_mm::destroy_distortion)> engine{
        surge_mm::create_distortion(), surge_mm::destroy_distortion};
    bool rack_compat{};

  public:
    SurgeDistortion()
    {
        params = surge_mm::distortion_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::distortion_samplerate(engine.get(), sr);
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
            params[14] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[14] > .5f)
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
        std::array<surge_mm::PolyValues, 12> cv{};
        for (unsigned i = 0; i < 12; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        surge_mm::process_distortion(engine.get(), params, cv, audio, channels, outputs);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-fx-history-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-fx-history-rack-v1")
        {
            rack_compat = true;
            surge_mm::distortion_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::distortion_rack_mode(engine.get(), false);
            params = surge_mm::distortion_defaults;
            surge_mm::reset_distortion(engine.get());
        }
    }
};
