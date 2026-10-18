// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "tuned_delay_dsp.h"
class SurgeTunedDelay : public UtilityProcessor<2, 4, 2>
{
    std::unique_ptr<surge_mm::TunedDelay, decltype(&surge_mm::destroy_tuned_delay)> engine{
        surge_mm::create_tuned_delay(), surge_mm::destroy_tuned_delay};

  public:
    SurgeTunedDelay()
    {
        params = {.5f, 0};
    }
    void set_samplerate(float sr) override
    {
        surge_mm::tuned_delay_samplerate(engine.get(), sr);
    }
    void update() override
    {
        if (bypassed)
        {
            for (unsigned s = 0; s < 2; ++s)
            {
                output_count(s, std::max(1u, unsigned(input_channels[s])));
                for (unsigned c = 0; c < 4; ++c)
                    outputs[s][c] = input(s, c, false);
            }
            return;
        }
        unsigned channels = 1;
        for (auto count : input_channels)
            channels = std::max(channels, std::min(4u, unsigned(count)));
        surge_mm::StereoPoly audio{};
        surge_mm::PolyValues pitch{}, correction{};
        for (unsigned c = 0; c < channels; ++c)
        {
            audio[0][c] = input(0, c);
            audio[1][c] = input(input_channels[1] ? 1 : 0, c);
            pitch[c] = (params[0] * 10 - 5 + 5) * 12 + input(2, c) * 12;
            correction[c] = std::round(std::clamp(params[1] + input(3, c) / 5, 0.f, 1.f) * 20);
        }
        surge_mm::process_tuned_delay(engine.get(), audio, pitch, correction, channels, outputs);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    void load_state(std::string_view state) override
    {
        if (state.empty())
        {
            params = {.5f, 0};
            surge_mm::reset_tuned_delay(engine.get());
        }
    }
};
