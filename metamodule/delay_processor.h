// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "delay_dsp.h"
#include <cstdio>
class SurgeDelay : public UtilityProcessor<12, 13, 2>
{
    std::unique_ptr<surge_mm::Delay, decltype(&surge_mm::destroy_delay)> engine{
        surge_mm::create_delay(), surge_mm::destroy_delay};
    std::array<std::atomic<float>, 2> times{};
    bool rack_compat{};

  public:
    SurgeDelay()
    {
        params = surge_mm::delay_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::delay_samplerate(engine.get(), sr);
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
        // Like Rack Delay, sum poly audio and use lane zero of modulation/clock.
        std::array<float, 2> audio{}, out{};
        for (unsigned s = 0; s < 2; ++s)
            for (unsigned c = 0; c < 4; ++c)
                audio[s] += input(s, c, false);
        if (!input_channels[1])
            audio[1] = audio[0];
        std::array<float, 10> cv{};
        for (unsigned i = 0; i < 10; ++i)
            cv[i] = input(3 + i, 0);
        surge_mm::process_delay(engine.get(), params, cv, audio, input(2, 0), input_channels[2] > 0,
                                out, rack_compat);
        auto display = surge_mm::delay_times(engine.get());
        for (unsigned s = 0; s < 2; ++s)
        {
            output_count(s, 1);
            outputs[s][0] = out[s];
            times[s].store(display[s], std::memory_order_relaxed);
        }
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-delay-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-delay-rack-v1")
        {
            rack_compat = true;
            surge_mm::delay_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::delay_rack_mode(engine.get(), false);
            params = surge_mm::delay_defaults;
            surge_mm::reset_delay(engine.get());
            for (auto &t : times)
                t.store(0, std::memory_order_relaxed);
        }
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        if (text.empty())
            return 0;
        int n = std::snprintf(text.data(), text.size(), "L %.3fs  R %.3fs",
                              times[0].load(std::memory_order_relaxed),
                              times[1].load(std::memory_order_relaxed));
        return n > 0 ? std::min<size_t>(n, text.size() - 1) : 0;
    }
};
