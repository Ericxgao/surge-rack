// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "unison_dsp.h"
#include <engine/Module.hpp>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>

// Firmware expander hookup recognizes rack::engine::Module. Keep just these
// two wrappers on that API; their sound engine and panel metadata remain native.
inline rack::plugin::Model *unison_model(bool expander)
{
    static rack::plugin::Plugin owner;
    static rack::plugin::Model helper, cv;
    static const bool initialized = [] {
        owner.slug = "SurgeXTNative";
        helper.plugin = cv.plugin = &owner;
        helper.slug = "UnisonHelper";
        helper.name = "Unison Helper";
        cv.slug = "UnisonHelperCVExpander";
        cv.name = "Unison CV Expander";
        return true;
    }();
    (void)initialized;
    return expander ? &cv : &helper;
}
inline float unison_input(const rack::engine::Input &input, unsigned lane, bool broadcast = false)
{
    if (broadcast && input.getChannels() == 1)
        lane = 0;
    float value = lane < unsigned(input.getChannels()) ? input.getVoltage(lane) : 0;
    return std::isfinite(value) ? value : 0;
}
inline size_t unison_status(std::span<char> text, unsigned status, bool expander)
{
    if (text.empty())
        return 0;
    int length;
    if (!status)
        length = std::snprintf(text.data(), text.size(), "%s", expander ? "NO\nLINK" : "Wire VCOs");
    else if (status & 0x10000)
        length =
            std::snprintf(text.data(), text.size(), "%s", expander ? "OVER\n16" : "Over 16 voices");
    else
        length = std::snprintf(text.data(), text.size(), expander ? "%ux%u" : "%u in x %u",
                               status & 255, (status >> 8) & 255);
    return length > 0 ? std::min<size_t>(length, text.size() - 1) : 0;
}
class SurgeUnisonHelper : public rack::engine::Module
{
    std::unique_ptr<surge_mm::Unison, decltype(&surge_mm::destroy_unison)> engine{
        surge_mm::create_unison(), surge_mm::destroy_unison};
    std::atomic<unsigned> status{};
    // The firmware can schedule the helper and expander on different cores.
    // Publish only the two routing dimensions as one atomic snapshot.
    std::atomic<unsigned> routing{1 + (3 << 8)};
    surge_mm::UnisonMap mapping;
    bool rack_compat{}, connection_changed{true}, rack_ready{};
    unsigned connection_banks{}, sample_position{};

  public:
    std::string save_state() override
    {
        return rack_compat ? "surge-unison-rack-v1" : "";
    }
    void load_state(std::string_view state) override
    {
        if (state != "surge-unison-rack-v1" && !state.empty())
            return;
        rack_compat = !state.empty();
        surge_mm::reset_unison(engine.get());
        connection_changed = true;
        rack_ready = false;
        connection_banks = sample_position = 0;
        mapping.configure(1, 3);
        routing.store(1 + (3 << 8), std::memory_order_relaxed);
        status.store(0, std::memory_order_relaxed);
        if (state.empty())
        {
            constexpr float defaults[]{.1f, 0, 0, 1, .25f, 0, .5f, 0, 0};
            for (unsigned i = 0; i < 9; ++i)
                params[i].setValue(defaults[i]);
        }
    }
    unsigned routing_snapshot() const
    {
        return routing.load(std::memory_order_relaxed);
    }
    SurgeUnisonHelper()
    {
        model = unison_model(false);
        config(9, 9, 6, 0);
        constexpr float defaults[]{.1f, 0, 0, 1, .25f, 0, .5f, 0, 0};
        for (unsigned id = 0; id < 9; ++id)
            configParam(id, 0, 1, defaults[id]);
        mapping.configure(1, 3);
    }
    void onSampleRateChange(const SampleRateChangeEvent &event) override
    {
        surge_mm::set_unison_samplerate(engine.get(), event.sampleRate);
    }
    void process(const ProcessArgs &) override
    {
        std::array<float, 9> controls{};
        for (unsigned id = 0; id < 9; ++id)
            controls[id] = std::clamp(params[id].getValue(), 0.f, 1.f);
        unsigned channels = std::clamp(inputs[0].getChannels(), 1, 4),
                 voices = 1 + unsigned(std::round(controls[4] * 8));
        const bool changed = mapping.channels != channels || mapping.voices != voices;
        if (changed)
            mapping.configure(channels, voices);
        routing.store(channels + (voices << 8), std::memory_order_relaxed);
        std::array<surge_mm::PolyValues, 9> audio{};
        for (unsigned port = 0; port < 9; ++port)
            for (unsigned lane = 0; lane < 4; ++lane)
                audio[port][lane] = unison_input(inputs[port], lane, port >= 5);
        bool ready = mapping.fits;
        for (unsigned bank = 0; bank < mapping.banks; ++bank)
            ready &= inputs[1 + bank].isConnected() && outputs[2 + bank].isConnected();
        if (rack_compat)
        {
            if (changed || connection_changed)
            {
                rack_ready = mapping.fits && channels * voices <= connection_banks * 4;
                connection_changed = false;
            }
            if (sample_position == 0)
            {
                unsigned banks = 0;
                while (banks < 4 && inputs[1 + banks].isConnected() &&
                       outputs[2 + banks].isConnected())
                    ++banks;
                connection_changed = banks != connection_banks;
                connection_banks = banks;
            }
            ready = rack_ready;
            sample_position = (sample_position + 1) % 8;
        }
        std::array<surge_mm::PolyValues, 6> result{};
        surge_mm::process_unison(engine.get(), controls, audio, mapping, outputs[1].isConnected(),
                                 ready, result, rack_compat);
        for (unsigned port = 0; port < 6; ++port)
        {
            outputs[port].voltages = result[port];
            outputs[port].setChannels(port < 2 ? mapping.channels
                                               : mapping.bank_channels[port - 2]);
        }
        status.store(!mapping.fits ? 0x10000
                     : ready       ? channels + (voices << 8)
                                   : 0,
                     std::memory_order_relaxed);
    }
    void processBypass(const ProcessArgs &) override
    {
        for (auto &output : outputs)
            output.voltages.fill(0);
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        return unison_status(text, status.load(std::memory_order_relaxed), false);
    }
};
class SurgeUnisonCVExpander : public rack::engine::Module
{
    std::atomic<unsigned> status{};
    surge_mm::UnisonMap map;
    SurgeUnisonHelper *source()
    {
        auto *neighbor = getLeftExpander().module;
        // Resolve each frame so deleting/reordering any intermediate expander
        // cannot leave a cached pointer to a removed helper.
        for (unsigned depth = 0; neighbor && depth < 64; ++depth)
        {
            if (neighbor == this)
                return nullptr;
            if (neighbor->getModel() == unison_model(false))
                return static_cast<SurgeUnisonHelper *>(neighbor);
            if (neighbor->getModel() != unison_model(true))
                return nullptr;
            neighbor = neighbor->getLeftExpander().module;
        }
        return nullptr;
    }

  public:
    SurgeUnisonCVExpander()
    {
        model = unison_model(true);
        config(0, 2, 8, 0);
    }
    void process(const ProcessArgs &) override
    {
        auto *helper = source();
        for (auto &output : outputs)
        {
            output.voltages.fill(0);
            output.setChannels(1);
        }
        if (!helper || helper->bypassed)
        {
            status.store(0, std::memory_order_relaxed);
            return;
        }
        unsigned routing = helper->routing_snapshot();
        unsigned channels = routing & 255, voices = (routing >> 8) & 255;
        if (!map.banks || map.channels != channels || map.voices != voices)
            map.configure(channels, voices);
        for (unsigned source = 0; source < 2; ++source)
        {
            surge_mm::PolyValues input{};
            for (unsigned lane = 0; lane < 4; ++lane)
                input[lane] = unison_input(inputs[source], lane);
            auto result = surge_mm::expand_unison_cv(map, input, inputs[source].getChannels());
            for (unsigned bank = 0; bank < 4; ++bank)
            {
                auto &output = outputs[source * 4 + bank];
                output.voltages = result[bank];
                output.setChannels(map.bank_channels[bank]);
            }
        }
        status.store(map.fits ? map.channels + (map.voices << 8) : 0x10000,
                     std::memory_order_relaxed);
    }
    void processBypass(const ProcessArgs &) override
    {
        for (auto &output : outputs)
            output.voltages.fill(0);
    }
    size_t get_display_text(int, std::span<char> text) override
    {
        return unison_status(text, status.load(std::memory_order_relaxed), true);
    }
};
