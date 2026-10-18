// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "tuned_delay_plus_dsp.h"
class SurgeTunedDelayPlus : public UtilityProcessor<12, 14, 2>
{
    std::unique_ptr<surge_mm::TunedDelayPlus, decltype(&surge_mm::destroy_tuned_delay_plus)> engine{
        surge_mm::create_tuned_delay_plus(), surge_mm::destroy_tuned_delay_plus};
    std::array<std::atomic<float>, 4> levels{};
    float falloff{.999f};
    unsigned meter_position{}, display_width{};
    std::span<uint32_t> pixels;
    bool rack_routing{};

  public:
    SurgeTunedDelayPlus()
    {
        params = surge_mm::tuned_plus_defaults;
    }
    void set_samplerate(float sr) override
    {
        if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
            return;
        surge_mm::tuned_delay_plus_samplerate(engine.get(), sr);
        falloff = std::exp(-2.0 * 3.14159265358979323846 * 8 / sr);
        meter_position = 0;
        for (auto &v : levels)
            v.store(0, std::memory_order_relaxed);
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
        if (rack_routing)
        {
            for (unsigned i = 0; i < 3; ++i)
                channels = std::max(channels, std::min(4u, unsigned(input_channels[i])));
        }
        else
            for (auto n : input_channels)
                channels = std::max(channels, std::min(4u, unsigned(n)));
        surge_mm::StereoPoly audio{}, feedback{};
        surge_mm::PolyValues pitch{}, excite{};
        std::array<surge_mm::PolyValues, 7> cv{};
        for (unsigned c = 0; c < channels; ++c)
        {
            audio[0][c] = input(0, c);
            audio[1][c] = input(input_channels[1] ? 1 : 0, c);
            feedback[0][c] = input(3, c);
            feedback[1][c] = input(input_channels[4] ? 4 : 3, c);
            pitch[c] = input(2, c, !rack_routing) + input(13, c);
            excite[c] = input(5, c, !rack_routing);
            for (unsigned i = 0; i < 7; ++i)
                cv[i][c] = input(6 + i, c);
        }
        surge_mm::process_tuned_delay_plus(engine.get(), params, cv, audio, feedback, pitch, excite,
                                           channels, outputs);
        for (unsigned s = 0; s < 2; ++s)
            output_count(s, channels);
        if (meter_position == 0)
            for (unsigned c = 0; c < 4; ++c)
                levels[c].store(
                    c < channels
                        ? std::clamp(std::max(falloff * levels[c].load(std::memory_order_relaxed),
                                              std::abs(outputs[0][c]) + std::abs(outputs[1][c])),
                                     0.f, 10.f)
                        : 0,
                    std::memory_order_relaxed);
        meter_position = (meter_position + 1) % 8;
    }
    std::string save_state() override
    {
        return rack_routing ? "surge-tuned-plus-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-tuned-plus-rack-v1")
            rack_routing = true;
        else if (state.empty())
        {
            rack_routing = false;
            params = surge_mm::tuned_plus_defaults;
            surge_mm::reset_tuned_delay_plus(engine.get());
            meter_position = 0;
            for (auto &v : levels)
                v.store(0, std::memory_order_relaxed);
        }
    }
    void show_graphic_display(int, std::span<uint32_t> buffer, unsigned width, lv_obj_t *) override
    {
        pixels = buffer;
        display_width = width;
    }
    void hide_graphic_display(int) override
    {
        pixels = {};
        display_width = 0;
    }
    bool draw_graphic_display(int) override
    {
        if (display_width < 4 || pixels.size() / display_width < 2)
            return false;
        unsigned height = pixels.size() / display_width;
        std::fill(pixels.begin(), pixels.end(), 0xff111111);
        for (unsigned c = 0; c < 4; ++c)
        {
            unsigned h = std::min(
                height, unsigned(levels[c].load(std::memory_order_relaxed) * height * .2f));
            for (unsigned y = height - h; y < height; ++y)
                for (unsigned x = c * display_width / 4; x < (c + 1) * display_width / 4 - 1; ++x)
                    pixels[y * display_width + x] = 0xffff9000;
        }
        return true;
    }
};
