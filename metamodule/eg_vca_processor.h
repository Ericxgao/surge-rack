// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "eg_vca_dsp.h"
class SurgeEGxVCA : public UtilityProcessor<17, 11, 4>
{
    std::unique_ptr<surge_mm::EGxVCA, decltype(&surge_mm::destroy_eg_vca)> engine{
        surge_mm::create_eg_vca(), surge_mm::destroy_eg_vca};
    std::array<std::atomic<float>, 64> history{};
    std::atomic<unsigned> history_head{};
    unsigned history_samples{};
    std::span<uint32_t> pixels;
    unsigned pixel_width{};
    bool rack_compat{};

  public:
    SurgeEGxVCA()
    {
        params[0] = params[1] = params[5] = .5f;
        params[3] = params[4] = params[6] = .1f;
        params[8] = params[9] = params[10] = .5f;
        params[12] = 1;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::set_eg_vca_samplerate(engine.get(), sr);
    }
    void update() override
    {
        std::array<surge_mm::PolyValues, 11> audio{};
        std::array<unsigned, 4> counts{};
        unsigned channels = 1;
        for (unsigned port = 0; port < 11; ++port)
            for (unsigned lane = 0; lane < 4; ++lane)
                audio[port][lane] = input(port, lane, port >= 2);
        for (unsigned port = 0; port < 4; ++port)
            counts[port] = std::min<unsigned>(input_channels[port], 4);
        surge_mm::process_eg_vca(engine.get(), params, audio, counts, outputs, channels,
                                 rack_compat);
        if (bypassed)
        {
            outputs = {};
            outputs[0] = audio[0];
            outputs[1] = audio[1];
        }
        for (unsigned port = 0; port < 4; ++port)
            output_count(port, channels);
        if (++history_samples == 64)
        {
            history_samples = 0;
            unsigned head = history_head.load(std::memory_order_relaxed);
            history[head].store(outputs[2][0], std::memory_order_relaxed);
            history_head.store((head + 1) % 64, std::memory_order_release);
        }
    }
    void show_graphic_display(int, std::span<uint32_t> buffer, unsigned width, lv_obj_t *) override
    {
        pixels = buffer;
        pixel_width = width;
    }
    bool draw_graphic_display(int) override
    {
        if (pixel_width < 2 || pixels.size() < 2 * pixel_width)
            return false;
        unsigned height = unsigned(pixels.size() / pixel_width),
                 head = history_head.load(std::memory_order_acquire);
        std::fill(pixels.begin(), pixels.end(), 0xff10151bu);
        int previous = int(height - 1);
        for (unsigned x = 0; x < pixel_width; ++x)
        {
            float value = std::clamp(
                history[(head + x * 63 / (pixel_width - 1)) % 64].load(std::memory_order_relaxed) /
                    10,
                0.f, 1.f);
            int y = int((1 - value) * (height - 1));
            if (!x)
                previous = y;
            for (int row = std::min(y, previous); row <= std::max(y, previous); ++row)
                pixels[row * pixel_width + x] = 0xffff9000u;
            previous = y;
        }
        return true;
    }
    void hide_graphic_display(int) override
    {
        pixels = {};
        pixel_width = 0;
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-eg-vca-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-eg-vca-rack-v1")
        {
            rack_compat = true;
            surge_mm::eg_vca_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::eg_vca_rack_mode(engine.get(), false);
            params = {.5, .5, 0, .1, .1, .5, .1, 0, .5, .5, .5, 0, 1, 0, 0, 0, 0};
            surge_mm::reset_eg_vca(engine.get());
            history_head = 0;
            history_samples = 0;
            for (auto &v : history)
                v = 0;
        }
    }
};
