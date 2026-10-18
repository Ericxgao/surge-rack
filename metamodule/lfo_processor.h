// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "lfo_dsp.h"
class SurgeLFO : public UtilityProcessor<surge_mm::lfo_parameter_count, 14, 6>
{
    std::unique_ptr<surge_mm::LFO, decltype(&surge_mm::destroy_lfo)> engine{surge_mm::create_lfo(),
                                                                            surge_mm::destroy_lfo};
    std::array<std::array<std::atomic<float>, 128>, 2> history{};
    std::atomic<unsigned> head{};
    unsigned history_samples{}, width{};
    std::span<uint32_t> pixels;
    bool rack_compat{};

  public:
    SurgeLFO()
    {
        params = surge_mm::lfo_defaults;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::lfo_samplerate(engine.get(), sr);
    }
    void update() override
    {
        if (bypassed)
        {
            for (auto &o : outputs)
                o.fill(0);
            return;
        }
        unsigned channels =
            params[53] < .5f && (input_channels[0] || input_channels[1])
                ? std::clamp<unsigned>(std::max(input_channels[0], input_channels[1]), 1, 4)
                : unsigned(std::round(params[52] * 3)) + 1;
        std::array<surge_mm::PolyValues, 14> values{};
        for (unsigned i = 0; i < 14; ++i)
            for (unsigned c = 0; c < 4; ++c)
                values[i][c] = input(i, c, false);
        surge_mm::process_lfo(engine.get(), params, values, input_channels, channels, outputs,
                              rack_compat);
        // Rack's event ports never call setChannels(), so connected cables are mono.
        for (unsigned i = 0; i < 6; ++i)
            output_count(i, rack_compat && i >= 3 ? 1 : channels);
        if (++history_samples == 128)
        {
            history_samples = 0;
            unsigned h = head.load(std::memory_order_relaxed);
            history[0][h].store(outputs[1][0], std::memory_order_relaxed);
            history[1][h].store(outputs[2][0], std::memory_order_relaxed);
            head.store((h + 1) % 128, std::memory_order_release);
        }
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-lfo-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-lfo-rack-v1")
        {
            rack_compat = true;
            surge_mm::lfo_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::lfo_rack_mode(engine.get(), false);
            params = surge_mm::lfo_defaults;
            surge_mm::reset_lfo(engine.get());
            for (auto &row : history)
                for (auto &v : row)
                    v.store(0, std::memory_order_relaxed);
            history_samples = 0;
            head.store(0, std::memory_order_release);
        }
    }
    void show_graphic_display(int, std::span<uint32_t> buffer, unsigned w, lv_obj_t *) override
    {
        pixels = buffer;
        width = w;
    }
    void hide_graphic_display(int) override
    {
        pixels = {};
        width = 0;
    }
    bool draw_graphic_display(int) override
    {
        if (width < 2 || pixels.size() < 2 * width)
            return false;
        unsigned height = pixels.size() / width, h = head.load(std::memory_order_acquire);
        std::fill(pixels.begin(), pixels.end(), 0xff10151b);
        for (unsigned x = 0; x < width; ++x)
            pixels[(height / 2) * width + x] = 0xff344052;
        for (unsigned curve = 0; curve < 2; ++curve)
        {
            int previous{};
            for (unsigned x = 0; x < width; ++x)
            {
                float value = history[curve][(h + x * 127 / (width - 1)) % 128].load(
                    std::memory_order_relaxed);
                int y = int((.5f - .5f * std::clamp(value / 10, -1.f, 1.f)) * (height - 1));
                if (!x)
                    previous = y;
                for (int row = std::min(y, previous); row <= std::max(y, previous); ++row)
                    pixels[row * width + x] = curve ? 0xff579de9 : 0xffff9000;
                previous = y;
            }
        }
        return true;
    }
};
