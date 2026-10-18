// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "envelope_dsp.h"
#include "quad_lfo_dsp.h"

class SurgeQuadLFO : public UtilityProcessor<19, 12, 4>
{
    std::unique_ptr<surge_mm::QuadLFO, decltype(&surge_mm::destroy_quad_lfo)> engine{
        surge_mm::create_quad_lfo(), surge_mm::destroy_quad_lfo};
    bool rack_compat{};

  public:
    SurgeQuadLFO()
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            params[i] = 5.f / 13;
            params[4 + i] = .5f;
            params[12 + i] = 1;
        }
    }
    void set_samplerate(float sr) override
    {
        surge_mm::set_quad_lfo_samplerate(engine.get(), sr);
    }
    void update() override
    {
        std::array<surge_mm::PolyValues, 12> audio{};
        std::array<unsigned, 4> counts{}, channels{};
        for (unsigned port = 0; port < 12; ++port)
            for (unsigned lane = 0; lane < 4; ++lane)
                audio[port][lane] = input(port, lane);
        for (unsigned port = 0; port < 4; ++port)
            counts[port] = std::min<unsigned>(input_channels[port], 4);
        if (rack_compat && std::round(params[16] * 4) != 0 && input_channels[3] == 1 &&
            !input_channels[2])
            for (unsigned c = 1; c < 4; ++c)
                audio[3][c] = 0;
        surge_mm::process_quad_lfo(engine.get(), params, audio, counts, outputs, channels,
                                   rack_compat);
        for (unsigned port = 0; port < 4; ++port)
        {
            output_count(port, channels[port]);
            if (bypassed)
                outputs[port].fill(0);
        }
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-quad-lfo-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-quad-lfo-rack-v1")
        {
            rack_compat = true;
            surge_mm::quad_lfo_rack_mode(engine.get(), true);
        }
        else if (state.empty())
        {
            rack_compat = false;
            surge_mm::quad_lfo_rack_mode(engine.get(), false);
            params.fill(0);
            for (unsigned i = 0; i < 4; ++i)
            {
                params[i] = 5.f / 13;
                params[4 + i] = .5f;
                params[12 + i] = 1;
            }
            surge_mm::reset_quad_lfo(engine.get());
        }
    }
};

class SurgeQuadAD : public UtilityProcessor<33, 12, 4>
{
    std::unique_ptr<surge_mm::QuadAD, decltype(&surge_mm::destroy_quad_ad)> engine{
        surge_mm::create_quad_ad(), surge_mm::destroy_quad_ad};
    bool rack_compat{};

  public:
    SurgeQuadAD()
    {
        for (unsigned i = 0; i < 8; ++i)
            params[i] = 3.f / surge_mm::ad_time_span;
        for (unsigned i = 12; i < 20; ++i)
            params[i] = .5f;
        for (unsigned i = 24; i < 28; ++i)
            params[i] = .5f;
    }
    void set_samplerate(float sr) override
    {
        surge_mm::set_quad_ad_samplerate(engine.get(), sr);
    }
    void update() override
    {
        std::array<surge_mm::PolyValues, 12> audio{};
        std::array<unsigned, 4> triggers{}, channels{};
        for (unsigned port = 0; port < 12; ++port)
            for (unsigned lane = 0; lane < 4; ++lane)
                audio[port][lane] = input(port, lane, port >= 4);
        for (unsigned port = 0; port < 4; ++port)
            triggers[port] = std::min<unsigned>(input_channels[port], 4);
        surge_mm::process_quad_ad(engine.get(), params, audio, triggers, outputs, channels,
                                  rack_compat);
        for (unsigned port = 0; port < 4; ++port)
        {
            if (bypassed)
            {
                outputs[port] = audio[port];
                channels[port] = std::max(1u, triggers[port]);
            }
            output_count(port, channels[port]);
        }
    }
    std::string save_state() override
    {
        return rack_compat ? "surge-quad-ad-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state == "surge-quad-ad-rack-v1")
            rack_compat = true;
        else if (state.empty())
        {
            rack_compat = false;
            params.fill(0);
            for (unsigned i = 0; i < 8; ++i)
                params[i] = 3.f / surge_mm::ad_time_span;
            for (unsigned i = 12; i < 20; ++i)
                params[i] = .5f;
            for (unsigned i = 24; i < 28; ++i)
                params[i] = .5f;
            surge_mm::reset_quad_ad(engine.get());
        }
    }
};

// src/ModMatrix.h / ModulationAssistant: destination range is 20 V,
// source scaling is 0.1, hence a full route adds twice its input voltage.
class SurgeModMatrix : public UtilityProcessor<49, 4, 8>
{
  public:
    SurgeModMatrix()
    {
        params.fill(.5f);
        for (unsigned i = 40; i < 49; ++i)
            params[i] = 0;
    }
    void update() override
    {
        // Preserve mono broadcasting when a converted destination has more
        // voices than its modulation sources. Existing patches default to 1.
        unsigned channels = 1 + unsigned(std::round(params[40] * 3));
        for (auto count : input_channels)
            channels = std::max(channels, std::min<unsigned>(count, 4));
        for (unsigned target = 0; target < 8; ++target)
        {
            output_count(target, channels);
            for (unsigned lane = 0; lane < channels; ++lane)
            {
                float value = params[target] * 20 - 10;
                for (unsigned source = 0; source < 4; ++source)
                    value += (params[8 + target * 4 + source] * 2 - 1) * 2 * input(source, lane);
                const auto curve = unsigned(std::round(params[41 + target] * 2));
                if (curve == 1)
                {
                    // Input 0..5 V encodes Rack's Alias Wave parameter. Keep
                    // modulation ahead of the discrete selection, as in VCO.h.
                    const float selected =
                        std::isfinite(value) ? std::clamp((value / 5 - .005f) / .99f, 0.f, 1.f) : 0;
                    const int wave = int(std::floor(selected * 17 + .5f));
                    // Rack excludes the five memory-reading modes. Use its
                    // nearest allowed Audio/Noise fallback for those holes.
                    constexpr unsigned native[]{0, 1, 2, 3, 5, 5,  5,  3,  3,
                                                5, 6, 7, 8, 9, 10, 11, 12, 4};
                    value = 5.f * native[wave] / 12;
                }
                else if (curve == 2)
                {
                    // Rack restricts the Unison knob to 1..9 voices, but its
                    // underlying integer parameter spans 1..16. Apply the
                    // integer endpoint correction after summing the CVs.
                    value =
                        std::isfinite(value)
                            ? 5.f * std::clamp((value / 5 * (8.f / 15) - .005f) / .99f, 0.f, 1.f)
                            : 0;
                }
                outputs[target][lane] = bypassed ? 0 : value;
            }
        }
    }
};
