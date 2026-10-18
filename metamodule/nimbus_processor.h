// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "nimbus_dsp.h"
class SurgeNimbus : public UtilityProcessor<13, 14, 2>
{
    std::unique_ptr<surge_mm::Nimbus, decltype(&surge_mm::destroy_nimbus)> engine{
        surge_mm::create_nimbus(), surge_mm::destroy_nimbus};
    bool rack_trigger{};

  public:
    SurgeNimbus()
    {
        params = surge_mm::nimbus_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::nimbus_samplerate(engine.get(), sr);
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
            params[12] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[12] > .5f)
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
        for (unsigned i = 0; i < 9; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        for (unsigned c = 0; c < channels; ++c)
            cv[9][c] = input(13, c);
        surge_mm::PolyValues freeze{}, trigger{};
        for (unsigned c = 0; c < channels; ++c)
        {
            freeze[c] = input(11, c);
            trigger[c] = input(12, c);
        }
        surge_mm::process_nimbus(engine.get(), params, cv, audio, channels, freeze, trigger,
                                 outputs, rack_trigger);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_trigger ? "surge-nimbus-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-nimbus-rack-v1")
        {
            surge_mm::prepare_nimbus_rack(engine.get());
            rack_trigger = true;
        }
        else if (state.empty())
        {
            rack_trigger = false;
            params = surge_mm::nimbus_defaults;
            surge_mm::reset_nimbus(engine.get());
        }
    }
};
