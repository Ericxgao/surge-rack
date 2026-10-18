// SPDX-License-Identifier: GPL-3.0-or-later
// Mixer routing adapted from src/Mixer.h, copyright 2019-2024 Surge contributors.
#pragma once
#include "CoreModules/CoreProcessor.hh"
#include "utility_dsp.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

template <unsigned Params, unsigned Inputs, unsigned Outputs>
class UtilityProcessor : public CoreProcessorPoly
{
  protected:
    std::array<float, Params> params{};
    std::array<surge_mm::PolyValues, Inputs> inputs{};
    std::array<surge_mm::PolyValues, Outputs> outputs{};
    std::array<uint8_t, Inputs> input_channels{};
    std::array<uint8_t, Outputs> output_channels{};
    float input(unsigned port, unsigned lane, bool broadcast = true) const
    {
        if (broadcast && input_channels[port] == 1)
            lane = 0;
        const float value =
            lane < std::min<unsigned>(input_channels[port], 4) ? inputs[port][lane] : 0.f;
        return std::isfinite(value) ? value : 0.f;
    }
    void output_count(unsigned port, unsigned channels)
    {
        if (output_channels[port])
            output_channels[port] = std::clamp(channels, 1u, 4u);
        for (unsigned lane = channels; lane < 4; ++lane)
            outputs[port][lane] = 0;
    }

  public:
    UtilityProcessor()
    {
        output_channels.fill(1);
    }
    void set_samplerate(float) override
    {
    }
    void set_param(int id, float value) override
    {
        if (id >= 0 && id < int(Params) && std::isfinite(value))
            params[id] = std::clamp(value, 0.f, 1.f);
    }
    float get_param(int id) const override
    {
        return id >= 0 && id < int(Params) ? params[id] : 0;
    }
    void set_input(int id, float value) override
    {
        if (id >= 0 && id < int(Inputs) && std::isfinite(value))
        {
            inputs[id].fill(0);
            inputs[id][0] = value;
            input_channels[id] = 1;
        }
    }
    float get_output(int id) const override
    {
        return id >= 0 && id < int(Outputs) ? outputs[id][0] : 0;
    }
    PolyPortBuffer get_poly_input_buffer(int id) override
    {
        return id >= 0 && id < int(Inputs) ? PolyPortBuffer{inputs[id].data(), &input_channels[id]}
                                           : PolyPortBuffer{};
    }
    PolyPortBuffer get_poly_output_buffer(int id) override
    {
        return id >= 0 && id < int(Outputs)
                   ? PolyPortBuffer{outputs[id].data(), &output_channels[id]}
                   : PolyPortBuffer{};
    }
    void mark_input_patched(int id) override
    {
        if (id >= 0 && id < int(Inputs) && !input_channels[id])
            input_channels[id] = 1;
    }
    void mark_input_unpatched(int id) override
    {
        if (id >= 0 && id < int(Inputs))
        {
            inputs[id].fill(0);
            input_channels[id] = 0;
        }
    }
    void mark_all_inputs_unpatched() override
    {
        for (unsigned id = 0; id < Inputs; ++id)
            mark_input_unpatched(id);
    }
    void mark_output_patched(int id) override
    {
        if (id >= 0 && id < int(Outputs) && !output_channels[id])
            output_channels[id] = 1;
    }
    void mark_output_unpatched(int id) override
    {
        if (id >= 0 && id < int(Outputs))
        {
            outputs[id].fill(0);
            output_channels[id] = 0;
        }
    }
    void mark_all_outputs_unpatched() override
    {
        for (unsigned id = 0; id < Outputs; ++id)
            mark_output_unpatched(id);
    }
};

class SurgeMixer : public UtilityProcessor<21, 14, 2>
{
    surge_mm::MixerNoise noise;
    std::array<bool, 3> ever_connected{};
    std::array<std::atomic<float>, 2> meters{};
    float falloff{std::exp(-2.f * 3.141592654f * 8.f / 48000.f)};
    std::span<uint32_t> pixels;
    unsigned pixel_width{};

  public:
    SurgeMixer()
    {
        params[0] = params[7] = 1;
        params[6] = .5f;
        for (unsigned i = 9; i < 14; ++i)
            params[i] = 1;
    }
    void set_samplerate(float sr) override
    {
        if (std::isfinite(sr) && sr >= 8000 && sr <= 192000)
            falloff = std::exp(-2.f * 3.141592654f * 8.f / sr);
    }
    void update() override
    {
        unsigned channels = 1;
        for (unsigned i = 0; i < 3; ++i)
        {
            if (!ever_connected[i] && (input_channels[2 * i] || input_channels[2 * i + 1]))
            {
                params[8 + i] = 0;
                ever_connected[i] = true;
            }
            channels = std::max(channels, std::min<unsigned>(input_channels[2 * i], 4));
        }
        std::array<bool, 6> routes{};
        bool any_solo = false;
        for (unsigned i = 0; i < 6; ++i)
            any_solo |= params[14 + i] > .5f;
        for (unsigned i = 0; i < 6; ++i)
            routes[i] = any_solo ? params[14 + i] > .5f : params[8 + i] < .5f;
        std::array<surge_mm::PolyValues, 6> audio{};
        std::array<surge_mm::PolyValues, 8> controls{};
        for (unsigned lane = 0; lane < channels; ++lane)
        {
            for (unsigned i = 0; i < 3; ++i)
            {
                bool mono = input_channels[2 * i] <= 1;
                audio[2 * i][lane] = input(2 * i, lane, mono);
                audio[2 * i + 1][lane] =
                    input(input_channels[2 * i + 1] ? 2 * i + 1 : 2 * i, lane, mono);
            }
            for (unsigned i = 0; i < 8; ++i)
            {
                const float value = std::clamp(params[i] + input(6 + i, lane) / 5.f, 0.f, 1.f);
                controls[i][lane] = i == 6 ? value * 2 - 1 : value;
            }
        }
        if (bypassed)
            for (auto &out : outputs)
                out.fill(0); // Rack mixer has no bypass routes
        else
            surge_mm::mix_sample(noise, audio, controls, routes, channels, outputs);
        const unsigned meter_lane = std::min(unsigned(std::round(params[20] * 3)), channels - 1);
        for (unsigned side = 0; side < 2; ++side)
        {
            output_count(side, channels);
            meters[side].store(
                std::min(10.f, std::max(std::abs(outputs[side][meter_lane]),
                                        falloff * meters[side].load(std::memory_order_relaxed))),
                std::memory_order_relaxed);
        }
    }
    // As in Rack, restored patches keep their saved mute values when connected.
    std::string save_state() override
    {
        return "surge-mixer-v1";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-mixer-v1")
            ever_connected.fill(true);
    }
    void show_graphic_display(int, std::span<uint32_t> buffer, unsigned width, lv_obj_t *) override
    {
        pixels = buffer;
        pixel_width = width;
    }
    void hide_graphic_display(int) override
    {
        pixels = {};
        pixel_width = 0;
    }
    bool draw_graphic_display(int) override
    {
        if (pixel_width < 2 || pixels.size() / pixel_width < 2)
            return false;
        unsigned height = pixels.size() / pixel_width;
        std::fill(pixels.begin(), pixels.end(), 0xff111111);
        for (unsigned side = 0; side < 2; ++side)
        {
            unsigned width =
                std::min(pixel_width,
                         unsigned(meters[side].load(std::memory_order_relaxed) * pixel_width / 10));
            for (unsigned y = side * height / 2; y < (side + 1) * height / 2; ++y)
                for (unsigned x = 0; x < width; ++x)
                    pixels[y * pixel_width + x] = x < pixel_width / 2 ? 0xff35cf62 : 0xffff9000;
        }
        return true;
    }
};

class SurgeDigitalRing : public UtilityProcessor<3, 11, 4>
{
    struct DeleteRing
    {
        void operator()(surge_mm::DigitalRing *p) const
        {
            surge_mm::destroy_digital_ring(p);
        }
    };
    std::unique_ptr<surge_mm::DigitalRing, DeleteRing> engine{surge_mm::create_digital_ring()};

  public:
    void set_samplerate(float sr) override
    {
        if (std::isfinite(sr) && sr >= 8000 && sr <= 192000)
            surge_mm::reset_digital_ring(engine.get());
    }
    void update() override
    {
        std::array<unsigned, 2> channels{1, 1};
        std::array<surge_mm::PolyValues, 8> audio{};
        std::array<surge_mm::PolyValues, 3> controls{};
        bool any_link = false;
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            for (unsigned i = 0; i < 3; ++i)
                controls[i][lane] = std::clamp(params[i] + input(8 + i, lane) / 5.f, 0.f, 1.f);
            any_link |= controls[2][lane] > .5f;
        }
        for (unsigned stage = 0; stage < 2; ++stage)
        {
            for (unsigned i = 0; i < 4; ++i)
                channels[stage] =
                    std::max(channels[stage], std::min<unsigned>(input_channels[stage * 4 + i], 4));
            for (unsigned lane = 0; lane < 4; ++lane)
                for (unsigned pair = 0; pair < 2; ++pair)
                {
                    const unsigned left = stage * 4 + pair * 2,
                                   right = input_channels[left + 1] ? left + 1 : left;
                    audio[left][lane] = input(left, lane, false);
                    audio[left + 1][lane] = input(right, lane, false);
                }
        }
        if (any_link)
            channels[1] = std::max(channels[0], channels[1]);
        if (bypassed)
        {
            for (unsigned stage = 0; stage < 2; ++stage)
                for (unsigned side = 0; side < 2; ++side)
                    for (unsigned lane = 0; lane < 4; ++lane)
                        outputs[stage * 2 + side][lane] = input(stage * 4 + side, lane, false);
        }
        else
            surge_mm::process_digital_ring(engine.get(), audio, controls, channels, outputs);
        for (unsigned stage = 0; stage < 2; ++stage)
            for (unsigned side = 0; side < 2; ++side)
                output_count(stage * 2 + side, channels[stage]);
    }
};
