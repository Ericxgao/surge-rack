// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "resonator_dsp.h"
#include <cstdio>
class SurgeResonator : public UtilityProcessor<19, 13, 2>
{
    std::unique_ptr<surge_mm::Resonator, decltype(&surge_mm::destroy_resonator)> engine{
        surge_mm::create_resonator(), surge_mm::destroy_resonator};
    std::array<std::atomic<float>, 6> display{};

  public:
    SurgeResonator()
    {
        params = surge_mm::resonator_defaults;
        for (unsigned i = 0; i < 3; ++i)
        {
            display[i] = params[3 * i];
            display[3 + i] = 0;
        }
    }
    void set_param(int id, float v) override
    {
        UtilityProcessor::set_param(id, v);
        if (id >= 0 && id < 9 && id % 3 == 0)
            display[id / 3].store(params[id], std::memory_order_relaxed);
        if (id >= 12 && id < 15)
            display[id - 9].store(params[id], std::memory_order_relaxed);
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        if (text.empty())
            return 0;
        std::array<float, 3> hz{};
        for (unsigned b = 0; b < 3; ++b)
        {
            bool ext = display[3 + b].load(std::memory_order_relaxed) > .5f;
            float lo = surge_mm::resonator_low[ext ? 0 : b],
                  hi = surge_mm::resonator_high[ext ? 2 : b];
            hz[b] =
                440 * std::exp2((lo + display[b].load(std::memory_order_relaxed) * (hi - lo)) / 12);
        }
        int length =
            std::snprintf(text.data(), text.size(), "%.0f/%.0f/%.0f Hz", hz[0], hz[1], hz[2]);
        return length > 0 ? std::min<size_t>(length, text.size() - 1) : 0;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::resonator_samplerate(engine.get(), sr);
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
            params[18] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[18] > .5f)
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
        std::array<surge_mm::PolyValues, 11> cv{};
        for (unsigned i = 0; i < 11; ++i)
            for (unsigned c = 0; c < channels; ++c)
                cv[i][c] = input(2 + i, c);
        surge_mm::process_resonator(engine.get(), params, cv, audio, channels, outputs);
        if (output_channels[0] && !output_channels[1])
            for (unsigned c = 0; c < channels; ++c)
                outputs[0][c] = .5f * (outputs[0][c] + outputs[1][c]);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
    }
    void load_state(std::string_view state) override
    {
        if (state.empty())
        {
            for (unsigned i = 0; i < 19; ++i)
                set_param(i, surge_mm::resonator_defaults[i]);
            surge_mm::reset_resonator(engine.get());
        }
    }
};
