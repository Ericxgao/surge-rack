// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "CoreModules/CoreProcessor.hh"
#include "engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

template <surge_mm::Model Model> class SurgeOscillator : public CoreProcessorPoly
{
  protected:
    // Panel input IDs are serialized patch coordinates. Keep them stable.
    // Port 11 is pitch CV for ordinary oscillators, audio for Alias/String.
    static constexpr int pitch_input = 0;
    static constexpr int reset_input = 1;
    static constexpr int drift_input = 9;
    static constexpr int level_input = 10;
    static constexpr int auxiliary_input = 11;
    static constexpr int separate_pitch_input = 12;
    static constexpr unsigned input_ports =
        Model == surge_mm::Model::Alias || Model == surge_mm::Model::String ? 13
        : surge_mm::separate_pitch_mod(Model)                               ? 12
                                                                            : 11;
    struct DestroyEngine
    {
        void operator()(surge_mm::Engine *e) const
        {
            surge_mm::destroy(e);
        }
    };
    std::array<std::unique_ptr<surge_mm::Engine, DestroyEngine>, MaxPolyChannels> voices{};
    surge_mm::Engine *engine{};
    std::array<std::array<float, MaxPolyChannels>, input_ports> inputs{};
    std::array<std::array<float, MaxPolyChannels>, 2> outputs{};
    std::array<uint8_t, input_ports> input_channels{};
    std::array<uint8_t, 2> output_channels{1, 1};
    surge_mm::OscillatorOutputSettings output_settings;
    bool rack_input_routing{};

  public:
    SurgeOscillator()
    {
        // Allocate outside the audio callback. Active cable channels alone run DSP.
        for (auto &voice : voices)
            voice.reset(surge_mm::create(Model));
        engine = voices[0].get();
    }
    void update() override
    {
        // Match Rack's pitch/reset-driven voice count, capped by the SDK.
        const unsigned channels = std::clamp<unsigned>(
            std::max({uint8_t{1}, input_channels[pitch_input], input_channels[reset_input]}), 1,
            MaxPolyChannels);
        for (auto &count : output_channels)
            if (count)
                count = channels;
        for (auto &out : outputs)
            std::fill(out.begin(), out.end(), 0.f);
        if (bypassed)
            return;
        for (unsigned channel = 0; channel < channels; ++channel)
        {
            if constexpr (Model == surge_mm::Model::Twist)
                surge_mm::set_trigger_connected(voices[channel].get(),
                                                input_channels[reset_input] > 0);
            for (int id = 0; id < int(input_ports); ++id)
            {
                const unsigned lane =
                    rack_input_routing && id == auxiliary_input &&
                            !surge_mm::separate_pitch_mod(Model)
                        ? 0
                    : input_channels[id] == 1 && !(rack_input_routing && id == pitch_input)
                        ? 0
                        : channel;
                const float value = lane < input_channels[id] ? inputs[id][lane] : 0.f;
                const int dsp_id = id == drift_input       ? surge_mm::drift_cv_input
                                   : id == level_input     ? surge_mm::level_cv_input
                                   : id == auxiliary_input ? (surge_mm::separate_pitch_mod(Model)
                                                                  ? surge_mm::pitch_mod_input
                                                                  : surge_mm::audio_input)
                                   : id == separate_pitch_input ? surge_mm::pitch_mod_input
                                                                : id;
                surge_mm::input(voices[channel].get(), dsp_id, value);
            }
            surge_mm::process(voices[channel].get(), outputs[0][channel], outputs[1][channel]);
            if (output_channels[0] && !output_channels[1])
                outputs[0][channel] = .5f * (outputs[0][channel] + outputs[1][channel]);
        }
    }
    void set_samplerate(float sr) override
    {
        for (auto &voice : voices)
            surge_mm::sample_rate(voice.get(), sr);
    }
    void set_param(int id, float value) override
    {
        if (id >= 0 && id < 10)
            for (auto &voice : voices)
                surge_mm::set_param(voice.get(), id, value);
        if constexpr (surge_mm::standard_oscillator(Model))
            if (id >= 10)
                for (auto &voice : voices)
                    surge_mm::set_standard_option(voice.get(), id - 10, value);
        if constexpr (Model == surge_mm::Model::Alias)
        {
            if (id >= 10 && id < 31)
                for (auto &voice : voices)
                    surge_mm::set_alias_option(voice.get(), id - 10, value);
            if (id == 31)
                for (auto &voice : voices)
                    surge_mm::set_octave_shift(voice.get(), value);
        }
        if constexpr (surge_mm::table_oscillator(Model))
        {
            constexpr unsigned extra = surge_mm::table_extra_param(Model);
            if (id >= 10 && id < int(extra + 2))
                for (auto &voice : voices)
                    surge_mm::set_wavetable_option(
                        voice.get(), id < int(extra) ? id - 10 : id - extra + 6, value);
        }
        if constexpr (Model == surge_mm::Model::Twist)
        {
            if (id >= 10 && id < 14)
                for (auto &voice : voices)
                    surge_mm::set_twist_option(voice.get(), id - 10, value);
            if (id == 14)
                for (auto &voice : voices)
                    surge_mm::set_octave_shift(voice.get(), value);
        }
        if constexpr (Model == surge_mm::Model::String)
        {
            if (id >= 10 && id < 19)
                for (auto &voice : voices)
                    surge_mm::set_string_option(voice.get(), id - 10, value);
            if (id == 19)
                for (auto &voice : voices)
                    surge_mm::set_octave_shift(voice.get(), value);
        }
    }
    float get_param(int id) const override
    {
        if (id >= 0 && id < 10)
            return surge_mm::get_param(engine, id);
        if constexpr (surge_mm::standard_oscillator(Model))
            return surge_mm::get_standard_option(engine, id - 10);
        if constexpr (Model == surge_mm::Model::Alias)
            return id == 31 ? surge_mm::get_octave_shift(engine)
                            : surge_mm::get_alias_option(engine, id - 10);
        if constexpr (surge_mm::table_oscillator(Model))
            return surge_mm::get_wavetable_option(
                engine, id < int(surge_mm::table_extra_param(Model))
                            ? id - 10
                            : id - surge_mm::table_extra_param(Model) + 6);
        if constexpr (Model == surge_mm::Model::Twist)
            return id == 14 ? surge_mm::get_octave_shift(engine)
                            : surge_mm::get_twist_option(engine, id - 10);
        if constexpr (Model == surge_mm::Model::String)
            return id == 19 ? surge_mm::get_octave_shift(engine)
                            : surge_mm::get_string_option(engine, id - 10);
        return 0;
    }
    void set_input(int id, float value) override
    {
        if (id < 0 || id >= int(input_ports) || !std::isfinite(value))
            return;
        inputs[id].fill(0);
        inputs[id][0] = value;
        input_channels[id] = 1;
    }
    float get_output(int id) const override
    {
        return id >= 0 && id < 2 ? outputs[id][0] : 0;
    }
    PolyPortBuffer get_poly_input_buffer(int id) override
    {
        return id >= 0 && id < int(input_ports)
                   ? PolyPortBuffer{inputs[id].data(), &input_channels[id]}
                   : PolyPortBuffer{};
    }
    PolyPortBuffer get_poly_output_buffer(int id) override
    {
        return id >= 0 && id < 2 ? PolyPortBuffer{outputs[id].data(), &output_channels[id]}
                                 : PolyPortBuffer{};
    }
    void mark_input_unpatched(int id) override
    {
        if (id >= 0 && id < int(input_ports))
        {
            inputs[id].fill(0);
            input_channels[id] = 0;
        }
    }
    void mark_all_inputs_unpatched() override
    {
        for (int id = 0; id < int(input_ports); ++id)
            mark_input_unpatched(id);
    }
    void mark_input_patched(int id) override
    {
        if (id >= 0 && id < int(input_ports) && !input_channels[id])
            input_channels[id] = 1;
    }
    void mark_output_unpatched(int id) override
    {
        if (id >= 0 && id < 2)
        {
            outputs[id].fill(0);
            output_channels[id] = 0;
        }
    }
    void mark_all_outputs_unpatched() override
    {
        for (int id = 0; id < 2; ++id)
            mark_output_unpatched(id);
    }
    void mark_output_patched(int id) override
    {
        if (id >= 0 && id < 2 && !output_channels[id])
            output_channels[id] = 1;
    }
    std::string save_state() override
    {
        if (!rack_input_routing && output_settings == surge_mm::OscillatorOutputSettings{})
            return {};
        return std::string(rack_input_routing ? "surge-osc-rack-v1 " : "surge-osc-output-v1 ") +
               char('0' + output_settings.halfband_order) + " " +
               char('0' + output_settings.steep) + " " + char('0' + output_settings.dc_block);
    }
    void load_state(std::string_view state) override
    {
        surge_mm::OscillatorOutputSettings next;
        const bool imported = state.starts_with("surge-osc-rack-v1 ");
        const std::string_view prefix = imported ? "surge-osc-rack-v1 " : "surge-osc-output-v1 ";
        if (!state.empty())
        {
            if (!state.starts_with(prefix) || state.size() != prefix.size() + 5)
                return;
            auto v = state.substr(prefix.size());
            if (v[0] < '1' || v[0] > '6' || v[1] != ' ' || v[3] != ' ' ||
                (v[2] != '0' && v[2] != '1') || (v[4] != '0' && v[4] != '1'))
                return;
            next = {unsigned(v[0] - '0'), v[2] == '1', v[4] == '1'};
        }
        output_settings = next;
        rack_input_routing = imported;
        for (auto &voice : voices)
        {
            surge_mm::configure_output(voice.get(), next);
            surge_mm::set_rack_input_routing(voice.get(), imported);
        }
    }
};
