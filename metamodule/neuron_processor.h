// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "neuron_dsp.h"
class SurgeNeuron : public UtilityProcessor<15, 15, 2>
{
    std::unique_ptr<surge_mm::Neuron, decltype(&surge_mm::destroy_neuron)> engine{
        surge_mm::create_neuron(), surge_mm::destroy_neuron};
    bool rack_clock{};

  public:
    SurgeNeuron()
    {
        params = surge_mm::neuron_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::neuron_samplerate(engine.get(), sr);
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
            params[13] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[13] > .5f)
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
        surge_mm::process_neuron(engine.get(), params, cv, audio, channels, input(14, 0),
                                 input_channels[14] > 0, outputs);
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
            surge_mm::neuron_rack_clock(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_clock = false;
            surge_mm::neuron_rack_clock(engine.get(), false);
            params = surge_mm::neuron_defaults;
            surge_mm::reset_neuron(engine.get());
        }
    }
};
