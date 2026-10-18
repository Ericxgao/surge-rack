// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "combulator_dsp.h"
#include <cstdio>
class SurgeCombulator : public UtilityProcessor<16, 14, 2>
{
    std::unique_ptr<surge_mm::Combulator, decltype(&surge_mm::destroy_combulator)> engine{
        surge_mm::create_combulator(), surge_mm::destroy_combulator};
    std::array<std::atomic<float>, 5> display{};
    bool rack_compat{};

  public:
    SurgeCombulator()
    {
        for (unsigned i = 0; i < 16; ++i)
            set_param(i, surge_mm::combulator_defaults[i]);
    }
    void set_param(int id, float v) override
    {
        UtilityProcessor::set_param(id, v);
        if (id >= 1 && id <= 3)
            display[id - 1].store(params[id], std::memory_order_relaxed);
        if (id == 13 || id == 14)
            display[id - 10].store(params[id], std::memory_order_relaxed);
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        if (text.empty())
            return 0;
        float pitch[3]{};
        for (unsigned b = 0; b < 3; ++b)
        {
            bool absolute = b == 0 || display[b + 2].load(std::memory_order_relaxed) > .5f;
            float lo = absolute ? surge_mm::combulator_low : -60,
                  hi = absolute ? surge_mm::combulator_high : 60;
            pitch[b] = lo + display[b].load(std::memory_order_relaxed) * (hi - lo);
            if (!absolute)
                pitch[b] += pitch[0];
        }
        int n = std::snprintf(text.data(), text.size(), "%.0f/%.0f/%.0f Hz",
                              440 * std::exp2(pitch[0] / 12), 440 * std::exp2(pitch[1] / 12),
                              440 * std::exp2(pitch[2] / 12));
        return n > 0 ? std::min<size_t>(n, text.size() - 1) : 0;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::combulator_samplerate(engine.get(), sr);
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
            params[15] > .5f
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : 1;
        surge_mm::StereoPoly audio{};
        if (params[15] > .5f)
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
        surge_mm::process_combulator(engine.get(), params, cv, audio, channels, outputs);
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
            surge_mm::combulator_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::combulator_rack_mode(engine.get(), false);
            for (unsigned i = 0; i < 16; ++i)
                set_param(i, surge_mm::combulator_defaults[i]);
            surge_mm::reset_combulator(engine.get());
        }
    }
};
