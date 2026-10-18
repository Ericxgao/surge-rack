// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "spring_dsp.h"
class SurgeSpringReverb : public UtilityProcessor<9, 11, 2>
{
    std::unique_ptr<surge_mm::SpringReverb, decltype(&surge_mm::destroy_spring)> engine{
        surge_mm::create_spring(), surge_mm::destroy_spring};
    std::array<bool, 4> gate_high{};
    bool button_high{}, rack_trigger{};

  public:
    SurgeSpringReverb()
    {
        params = surge_mm::spring_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::spring_samplerate(engine.get(), sr);
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
            params[8] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        std::array<bool, 4> knock{};
        // Imported Rack triggers are sampled by the DSP at its block boundary.
        if (!rack_trigger)
        {
            bool button = params[6] > .5f, pressed = button && !button_high;
            button_high = button;
            for (unsigned c = 0; c < 4; ++c)
            {
                float gate = input(9, c);
                bool rise = gate >= 1 && !gate_high[c];
                if (gate >= 1)
                    gate_high[c] = true;
                else if (gate <= .1f)
                    gate_high[c] = false;
                knock[c] = rise || pressed;
            }
        }
        surge_mm::StereoPoly audio{};
        if (params[8] > .5f)
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
        std::array<surge_mm::PolyValues, 8> cv{};
        for (unsigned i = 0; i < 7; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        for (unsigned c = 0; c < channels; ++c)
            cv[7][c] = input(10, c);
        surge_mm::PolyValues gates{};
        for (unsigned c = 0; c < channels; ++c)
            gates[c] = input(9, c);
        surge_mm::process_spring(engine.get(), params, cv, knock, audio, channels, outputs, gates,
                                 rack_trigger);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    std::string save_state() override
    {
        return rack_trigger ? "surge-spring-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-spring-rack-v1")
        {
            surge_mm::spring_rack_mode(engine.get(), true);
            rack_trigger = true;
        }
        else if (state.empty())
        {
            rack_trigger = false;
            surge_mm::spring_rack_mode(engine.get(), false);
            params = surge_mm::spring_defaults;
            gate_high.fill(false);
            button_high = false;
            surge_mm::reset_spring(engine.get());
        }
    }
};
