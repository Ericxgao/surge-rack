// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_processor.h"
#include "filter_dsp.h"
#include <cstdio>
template <surge_mm::FilterModule Kind>
class SurgeFilter : public UtilityProcessor<Kind == surge_mm::FilterModule::VCF ? 7 : 9, 7, 2>
{
    static constexpr bool vcf = Kind == surge_mm::FilterModule::VCF;
    using Base = UtilityProcessor<vcf ? 7 : 9, 7, 2>;
    using Base::params;
    using Base::inputs;
    using Base::outputs;
    using Base::input_channels;
    std::unique_ptr<surge_mm::Filter, decltype(&surge_mm::destroy_filter)> engine{
        surge_mm::create_filter(Kind), surge_mm::destroy_filter};
    std::array<std::atomic<unsigned>, 2> display_mode{};
    bool rack_import{};

  public:
    SurgeFilter()
    {
        auto defaults = surge_mm::filter_defaults(Kind);
        for (unsigned i = 0; i < params.size(); ++i)
            set_param(i, defaults[i]);
    }
    void set_samplerate(float sr) override
    {
        surge_mm::filter_samplerate(engine.get(), sr);
    }
    void set_param(int id, float value) override
    {
        Base::set_param(id, value);
        if (id == 5)
            display_mode[0].store(
                unsigned(std::round(params[5] * (surge_mm::filter_model_count(Kind) - 1))),
                std::memory_order_relaxed);
        if constexpr (vcf)
            if (id == 6)
                display_mode[1].store(unsigned(std::round(params[6] * 11)),
                                      std::memory_order_relaxed);
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        if (text.empty())
            return 0;
        auto type = display_mode[0].load(std::memory_order_relaxed);
        auto sub = std::min(display_mode[1].load(std::memory_order_relaxed),
                            surge_mm::filter_subtype_count(type) - 1);
        std::string label = surge_mm::filter_model_name(Kind, type);
        if constexpr (vcf)
            label += "\n" + surge_mm::filter_subtype_name(type, sub);
        auto count = std::min(text.size() - 1, label.size());
        std::copy_n(label.data(), count, text.data());
        text[count] = 0;
        return count;
    }
    void update() override
    {
        if constexpr (vcf)
            if (rack_import && !this->bypassed)
            {
                auto type =
                    unsigned(std::round(params[5] * (surge_mm::filter_model_count(Kind) - 1)));
                auto subtype = unsigned(std::round(params[6] * 11));
                // Rack clears an invalid subtype after a model change. Also clear
                // its erroneous count-valued endpoint instead of calling the SST
                // engine with an out-of-range subtype.
                if (subtype >= surge_mm::filter_subtype_count(type))
                    set_param(6, 0);
            }
        std::array<unsigned, 2> count{std::min<unsigned>(input_channels[0], 4),
                                      std::min<unsigned>(input_channels[1], 4)};
        surge_mm::StereoPoly audio{};
        for (unsigned side = 0; side < 2; ++side)
        {
            this->output_count(side, std::max(1u, count[side]));
            for (unsigned c = 0; c < count[side]; ++c)
                audio[side][c] = this->input(side, c, false);
        }
        if (this->bypassed)
        {
            outputs = audio;
            return;
        }
        std::array<surge_mm::PolyValues, 5> cv{};
        for (unsigned i = 0; i < 5; ++i)
            for (unsigned c = 0; c < 4; ++c)
                cv[i][c] = this->input(2 + i, c);
        std::array<float, 9> p{};
        std::copy(params.begin(), params.end(), p.begin());
        surge_mm::process_filter(engine.get(), p, cv, audio, count, outputs);
    }
    void load_state(std::string_view state) override
    {
        if constexpr (vcf)
            rack_import = state == "surge-vcf-rack-v1";
        if (state.empty())
        {
            auto defaults = surge_mm::filter_defaults(Kind);
            for (unsigned i = 0; i < params.size(); ++i)
                set_param(i, defaults[i]);
            surge_mm::reset_filter(engine.get());
        }
    }
    std::string save_state() override
    {
        return rack_import ? "surge-vcf-rack-v1" : "";
    }
};
using SurgeVCF = SurgeFilter<surge_mm::FilterModule::VCF>;
using SurgeWaveshaper = SurgeFilter<surge_mm::FilterModule::Waveshaper>;
