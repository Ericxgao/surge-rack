// SPDX-License-Identifier: GPL-3.0-or-later
// Rack oscillator layouts, with direct CV jacks in place of assignment controls.
#pragma once
#include "panel_controls.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include "oscillator_layouts.h"

inline constexpr std::array<const char *, 13> alias_shape_names{
    "Sine", "Ramp", "Pulse", "Noise", "Additive", "Audio In", "TX 2",
    "TX 3", "TX 4", "TX 5",  "TX 6",  "TX 7",     "TX 8"};

template <surge_mm::Model Model> class OscillatorDirect : public SurgeOscillator<Model>
{
    using SurgeOscillator<Model>::engine;
    static constexpr unsigned params = surge_mm::standard_oscillator(Model)
                                           ? 10 + surge_mm::standard_option_count(Model)
                                       : Model == surge_mm::Model::Alias     ? 32
                                       : Model == surge_mm::Model::String    ? 20
                                       : Model == surge_mm::Model::Twist     ? 15
                                       : Model == surge_mm::Model::Wavetable ? 16
                                       : Model == surge_mm::Model::Window    ? 18
                                                                             : 10;
    std::array<std::atomic<float>, params> display_params{};
    std::array<std::atomic<float>, 3> selector_cv{};
    std::array<float, params> last_display_params{};
    surge_mm::Engine *preview{};
    std::span<uint32_t> pixels;
    unsigned pixel_width{};
    bool dirty{true};

  protected:
    virtual void prepare_preview(surge_mm::Engine *)
    {
    }
    void invalidate_preview()
    {
        dirty = true;
    }

  public:
    OscillatorDirect()
    {
        for (int i = 0; i < int(params); ++i)
            display_params[i].store(SurgeOscillator<Model>::get_param(i));
    }
    ~OscillatorDirect() override
    {
        if (preview)
            surge_mm::destroy(preview);
    }
    void set_param(int id, float v) override
    {
        if (id < 0 || id >= int(params) || !std::isfinite(v))
            return;
        SurgeOscillator<Model>::set_param(id, v);
        display_params[id].store(SurgeOscillator<Model>::get_param(id), std::memory_order_relaxed);
    }
    float get_param(int id) const override
    {
        return SurgeOscillator<Model>::get_param(id);
    }
    void update() override
    {
        SurgeOscillator<Model>::update();
        if constexpr (Model == surge_mm::Model::Alias || Model == surge_mm::Model::Sine ||
                      Model == surge_mm::Model::Twist || Model == surge_mm::Model::String)
        {
            selector_cv[0].store(this->inputs[2][0] / 5, std::memory_order_relaxed);
        }
        if constexpr (Model == surge_mm::Model::Alias || surge_mm::table_oscillator(Model) ||
                      (surge_mm::standard_oscillator(Model) && !surge_mm::fm_oscillator(Model)))
        {
            selector_cv[1].store(this->inputs[8][0] / 5, std::memory_order_relaxed);
        }
        if constexpr (Model == surge_mm::Model::Sine)
            selector_cv[2].store(this->inputs[4][0] / 5, std::memory_order_relaxed);
        if constexpr (Model == surge_mm::Model::Window)
            selector_cv[2].store(this->inputs[4][0] / 5, std::memory_order_relaxed);
        if constexpr (Model == surge_mm::Model::FM2)
        {
            selector_cv[0].store(this->inputs[3][0] / 5, std::memory_order_relaxed);
            selector_cv[2].store(this->inputs[5][0] / 5, std::memory_order_relaxed);
        }
    }
    float selector_value(unsigned id) const
    {
        return std::clamp(display_params[id].load(std::memory_order_relaxed) +
                              selector_cv[Model == surge_mm::Model::FM2 ? (id == 2 ? 0 : 2)
                                          : id == 1                     ? 0
                                          : id == 3                     ? 2
                                                                        : 1]
                                  .load(std::memory_order_relaxed),
                          0.f, 1.f);
    }
    size_t get_display_text(int id, std::span<char> text) override
    {
        if (text.empty())
            return 0;
        text[0] = 0;
        if constexpr (Model == surge_mm::Model::Alias)
        {
            if (id != 1)
                return 0;
            auto shape = unsigned(std::round(selector_value(1) * 12));
            auto voices = 1 + unsigned(std::round(selector_value(7) * 15));
            int length = std::snprintf(text.data(), text.size(), "%s  x%u",
                                       alias_shape_names[shape], voices);
            return length > 0 ? std::min<size_t>(length, text.size() - 1) : 0;
        }
        return 0;
    }
    void show_graphic_display(int, std::span<uint32_t> buffer, unsigned width, lv_obj_t *) override
    {
        pixels = buffer;
        pixel_width = width;
        dirty = true;
        if (!preview)
            preview = surge_mm::create(Model);
    }
    void hide_graphic_display(int) override
    {
        pixels = {};
        pixel_width = 0;
    }
    bool draw_graphic_display(int) override
    {
        if (!preview || pixel_width < 2 || pixels.size() / pixel_width < 3)
            return false;
        std::array<float, params> values{};
        for (int i = 0; i < int(params); ++i)
            values[i] = display_params[i].load(std::memory_order_relaxed);
        if constexpr (Model == surge_mm::Model::Alias)
            values[1] = std::round(selector_value(1) * 12) / 12;
        if constexpr (Model == surge_mm::Model::Sine)
        {
            values[1] = std::round(selector_value(1) * 27) / 27;
            values[3] = std::round(selector_value(3));
        }
        if constexpr (Model == surge_mm::Model::Alias || surge_mm::table_oscillator(Model) ||
                      (surge_mm::standard_oscillator(Model) && !surge_mm::fm_oscillator(Model)))
            values[7] = std::round(selector_value(7) * 15) / 15;
        if constexpr (Model == surge_mm::Model::FM2)
        {
            values[2] = std::round(selector_value(2) * 31) / 31;
            values[4] = std::round(selector_value(4) * 31) / 31;
        }
        if constexpr (Model == surge_mm::Model::Twist)
            values[1] = std::round(selector_value(1) * 15) / 15;
        if constexpr (Model == surge_mm::Model::String)
            values[1] = std::round(selector_value(1) * 14) / 14;
        if constexpr (Model == surge_mm::Model::Window)
            values[3] = std::round(selector_value(3) * 8) / 8;
        if (!dirty && values == last_display_params)
            return false;
        dirty = false;
        last_display_params = values;
        prepare_preview(preview);
        // A separate Surge instance renders a parameter preview in the GUI thread.
        // It never reads or changes the sounding oscillator's state.
        for (int i = 0; i < 10; ++i)
            surge_mm::set_param(preview, i, values[i]);
        if constexpr (surge_mm::standard_oscillator(Model))
            for (int i = 10; i < int(params); ++i)
                surge_mm::set_standard_option(preview, i - 10, values[i]);
        if constexpr (Model == surge_mm::Model::Alias)
        {
            for (int i = 10; i < 31; ++i)
                surge_mm::set_alias_option(preview, i - 10, values[i]);
            surge_mm::set_octave_shift(preview, values[31]);
        }
        if constexpr (Model == surge_mm::Model::String)
        {
            for (int i = 10; i < 19; ++i)
                surge_mm::set_string_option(preview, i - 10, values[i]);
            surge_mm::set_octave_shift(preview, values[19]);
        }
        if constexpr (surge_mm::table_oscillator(Model))
        {
            constexpr unsigned extra = surge_mm::table_extra_param(Model);
            for (int i = 10; i < int(params); ++i)
                surge_mm::set_wavetable_option(preview, i < int(extra) ? i - 10 : i - extra + 6,
                                               values[i]);
        }
        if constexpr (Model == surge_mm::Model::Twist)
        {
            for (int i = 10; i < 14; ++i)
                surge_mm::set_twist_option(preview, i - 10, values[i]);
            surge_mm::set_twist_option(preview, 0, .5f); // continuous waveform preview
            surge_mm::set_octave_shift(preview, values[14]);
        }
        surge_mm::set_param(preview, 8, 0); // steady preview, like Rack's display oscillator
        surge_mm::set_param(preview, 9, 1);
        surge_mm::sample_rate(preview, 48000);
        float l{}, r{}, prev{};
        for (int i = 0; i < 4096; ++i)
            surge_mm::process(preview, l, r);
        for (int i = 0; i < 4096; ++i)
        {
            prev = l;
            surge_mm::process(preview, l, r);
            if (prev <= 0 && l > 0)
                break;
        }
        const int height = int(pixels.size() / pixel_width);
        std::fill(pixels.begin(), pixels.end(), 0xff111111u);
        for (unsigned x = 0; x < pixel_width; ++x)
            pixels[(height / 2) * pixel_width + x] = 0xff303030u;
        float octave = 0;
        if constexpr (Model == surge_mm::Model::Alias)
            octave = (values[31] - .5f) * 6;
        if constexpr (surge_mm::standard_oscillator(Model))
            octave = (values[10] - .5f) * 6;
        if constexpr (Model == surge_mm::Model::Twist)
            octave = (values[14] - .5f) * 6;
        if constexpr (Model == surge_mm::Model::String)
            octave = (values[19] - .5f) * 6;
        if constexpr (surge_mm::table_oscillator(Model))
            octave = (values[surge_mm::table_extra_param(Model)] - .5f) * 6;
        const float hz = 261.625565f * std::exp2((values[0] - .5f) * 10.f + octave);
        const int samples = std::clamp(int(96000.f / hz), 32, 2048);
        int previous_y = height / 2, done = 0;
        for (unsigned x = 0; x < pixel_width; ++x)
        {
            const int until = int((x + 1) * samples / pixel_width);
            while (done < until)
            {
                surge_mm::process(preview, l, r);
                ++done;
            }
            const int y = std::clamp(int(height * .5f - l / 5.f * height * .4f), 1, height - 2);
            for (int yy = std::min(y, previous_y); yy <= std::max(y, previous_y); ++yy)
                pixels[yy * pixel_width + x] = 0xffff9000u;
            previous_y = y;
        }
        return true;
    }
};

#include "table_oscillator.h"

template <surge_mm::Model Model>
void register_direct_oscillator(const char *slug, const char *panel)
{
    using namespace MetaModule;
    constexpr bool alias = Model == surge_mm::Model::Alias,
                   string = Model == surge_mm::Model::String,
                   twist = Model == surge_mm::Model::Twist;
    constexpr bool window = Model == surge_mm::Model::Window,
                   table = window || Model == surge_mm::Model::Wavetable;
    constexpr unsigned count = surge_mm::standard_oscillator(Model)
                                   ? 25 + surge_mm::standard_option_count(Model)
                               : alias  ? 49
                               : string ? 36
                               : twist  ? 30
                               : window ? 33
                               : table  ? 31
                                        : 24;
    static std::array<Element, count> elements;
    static std::array<ElementCount::Indices, count> indices;
    constexpr auto slots = oscillator_layout<Model>();
    constexpr std::array<std::array<float, 2>, 11> jack_xy{{{6.5f, 114.5f},
                                                            {18.5f, 114.5f},
                                                            {9.48f, 85},
                                                            {23.48f, 85},
                                                            {37.48f, 85},
                                                            {51.48f, 85},
                                                            {9.48f, 99},
                                                            {23.48f, 99},
                                                            {37.48f, 99},
                                                            {51.48f, 99},
                                                            {30.5f, 114.5f}}};
    static std::array<std::string, 11> jack_names;
    auto defaults = surge_mm::create(Model);
    jack_names[0] = "V/Oct";
    jack_names[1] = (string || twist) ? "Trigger" : "Reset";
    for (int i = 2; i < 11; ++i)
        jack_names[i] = std::string(surge_mm::param_name(defaults, i - 1)) + " CV";
    for (int i = 0; i < 10; ++i)
    {
        Knob knob;
        if (i < 8)
        {
            knob.x_mm = 9.48f + (slots[i] % 4) * 14;
            knob.y_mm = 55 + (slots[i] / 4) * 16;
        }
        else
        {
            knob.x_mm = i == 8 ? 18 : 44;
            knob.y_mm = 42;
        }
        knob.short_name = surge_mm::param_name(defaults, i);
        knob.image = i < 8 ? "SurgeXTNative/components/rack-knob.png"
                           : "SurgeXTNative/components/rack-small-knob.png";
        knob.default_value = surge_mm::get_param(defaults, i);
        knob.display_mult = 100;
        knob.units = "%";
        if (i == 0)
        {
            knob.display_mult = 120;
            knob.display_offset = -60;
            knob.units = "st";
        }
        if (i == 7 && Model != surge_mm::Model::FM2 && Model != surge_mm::Model::FM3 && !string &&
            !twist)
        {
            knob.display_mult = 15;
            knob.display_offset = 1;
            knob.units = "voices";
        }
        elements[i] = knob;
        indices[i] = ElementCount::NoElementIndices;
        indices[i].param_idx = i;
    }
    for (int i = 0; i < 11; ++i)
    {
        JackInput jack;
        jack.x_mm = jack_xy[i][0];
        jack.y_mm = jack_xy[i][1];
        jack.short_name = jack_names[i];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[10 + i] = jack;
        indices[10 + i] = ElementCount::NoElementIndices;
        indices[10 + i].input_idx = i;
    }
    for (int i = 0; i < 2; ++i)
    {
        JackOutput jack;
        jack.x_mm = 42.5f + i * 12;
        jack.y_mm = 114.5f;
        jack.short_name = i == 0 ? "Left" : "Right";
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[21 + i] = jack;
        indices[21 + i] = ElementCount::NoElementIndices;
        indices[21 + i].output_idx = i;
    }
    DynamicGraphicDisplay plot;
    plot.x_mm = 5.5f;
    plot.y_mm = 10;
    plot.coords = Coords::TopLeft;
    plot.width_mm = 50;
    plot.height_mm = 25;
    plot.short_name = "Waveform";
    elements[23] = plot;
    indices[23] = ElementCount::NoElementIndices;
    indices[23].light_idx = 0;
    if constexpr (surge_mm::standard_oscillator(Model))
    {
        JackInput pitch_mod;
        pitch_mod.x_mm = 30.5f;
        pitch_mod.y_mm = 42;
        pitch_mod.short_name = "Pitch Mod CV";
        pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
        elements[24] = pitch_mod;
        indices[24] = ElementCount::NoElementIndices;
        indices[24].input_idx = 11;
        static const char *names[]{"Octave",      "Detune Range", "Detune Type", "Character",
                                   "Phase Reset", "Multi Shape",  "Multi Sub",   "Sub Skips Sync"};
        constexpr unsigned option_defaults[]{3, 0, 0, 1, 1, 0, 0, 0};
        for (unsigned i = 0; i < surge_mm::standard_option_count(Model); ++i)
        {
            AltParamChoiceLabeled option;
            option.short_name = i < 8 ? names[i] : "Feedback Range";
            option.default_value = i < 8 ? option_defaults[i] : 0;
            option.num_pos = i == 0 ? 7 : (i == 3 || i == 5) ? 3 : 2;
            if (i == 0)
                option.pos_names = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
            if (i == 1)
                option.pos_names = {"100 cents", "1200 cents"};
            if (i == 2)
                option.pos_names = {"Relative", "Absolute"};
            if (i == 3)
                option.pos_names = {"Warm", "Off", "Bright"};
            if (i == 4)
                option.pos_names = {"Random", "Zero"};
            if (i == 5)
                option.pos_names = {"Triangle", "Square", "Sine"};
            if (i >= 6)
                option.pos_names = {"Off", "On"};
            if constexpr (Model == surge_mm::Model::Sine || Model == surge_mm::Model::SHNoise)
            {
                if (i == 5 || i == 6)
                {
                    option.short_name = i == 5 ? "Low Cut Enable" : "High Cut Enable";
                    option.num_pos = 3;
                    option.pos_names = {"Auto (frequency)", "Off", "On"};
                }
                if (i == 7)
                {
                    option.short_name = "Vintage Feedback";
                    option.num_pos = 2;
                    option.pos_names = {"Off", "On"};
                }
                if (i == 8)
                {
                    option.num_pos = 2;
                    option.pos_names = {"Normal", "Extended (4x)"};
                }
            }
            if constexpr (surge_mm::fm_oscillator(Model))
            {
                if (i == 1)
                {
                    option.short_name = "Feedback Range";
                    option.pos_names = {"Normal", "Extended (4x)"};
                }
                if (i == 2)
                {
                    option.short_name = "Vintage Feedback";
                    option.pos_names = {"Off", "On"};
                }
                if (i == 5 || i == 6)
                {
                    option.short_name = i == 5 ? "M1 Tuning" : "M2 Tuning";
                    option.num_pos = 2;
                    option.pos_names = {"Ratio", "Absolute Frequency"};
                }
            }
            elements[25 + i] = option;
            indices[25 + i] = ElementCount::NoElementIndices;
            indices[25 + i].param_idx = 10 + i;
        }
        if constexpr (Model == surge_mm::Model::Sine)
        {
            for (unsigned id : {1u, 3u})
            {
                KnobSnapped knob;
                knob.x_mm = 9.48f + (slots[id] % 4) * 14;
                knob.y_mm = 55 + (slots[id] / 4) * 16;
                knob.short_name = id == 1 ? "Shape" : "Behavior";
                knob.num_pos = id == 1 ? 28 : 2;
                knob.default_value = id == 1 ? 0 : 1;
                static std::array<std::string, 28> shapes;
                if (id == 1)
                    for (unsigned i = 0; i < 28; ++i)
                    {
                        shapes[i] = i == 0 ? "Sine" : "Shape " + std::to_string(i + 1);
                        knob.pos_names[i] = shapes[i].c_str();
                    }
                else
                    knob.pos_names = {"Legacy", "Modern"};
                knob.image = "SurgeXTNative/components/rack-knob.png";
                elements[id] = knob;
            }
        }
        if constexpr (Model == surge_mm::Model::FM2)
            for (unsigned id : {2u, 4u})
            {
                KnobSnapped knob;
                knob.x_mm = 9.48f + (slots[id] % 4) * 14;
                knob.y_mm = 55 + (slots[id] / 4) * 16;
                knob.short_name = id == 2 ? "M1 Ratio" : "M2 Ratio";
                knob.num_pos = 32;
                knob.default_value = surge_mm::get_param(defaults, id) * 31;
                static std::array<std::string, 30> ratios;
                for (unsigned i = 0; i < ratios.size(); ++i)
                {
                    ratios[i] = std::to_string(i + 1);
                    knob.pos_names[i] = ratios[i].c_str();
                }
                knob.image = "SurgeXTNative/components/rack-knob.png";
                elements[id] = knob;
            }
    }
    if constexpr (alias)
    {
        AltParamChoiceLabeled shape;
        shape.short_name = "Shape";
        shape.default_value = 0;
        shape.num_pos = 13;
        for (unsigned i = 0; i < 13; ++i)
            shape.pos_names[i] = alias_shape_names[i];
        elements[1] = shape;
        AltParamChoiceLabeled voices;
        voices.short_name = "Voices";
        voices.default_value = 0;
        voices.num_pos = 16;
        static std::array<std::string, 16> voice_names;
        for (unsigned i = 0; i < 16; ++i)
        {
            voice_names[i] = std::to_string(i + 1);
            voices.pos_names[i] = voice_names[i];
        }
        elements[7] = voices;
        for (unsigned id = 8; id < 10; ++id)
        {
            AltParamContinuous option;
            option.short_name = surge_mm::param_name(defaults, id);
            option.default_value = surge_mm::get_param(defaults, id);
            elements[id] = option;
        }
        JackInput audio;
        audio.x_mm = 23.48f;
        audio.y_mm = 71;
        audio.short_name = "Audio In";
        audio.image = "SurgeXTNative/components/rack-jack.png";
        elements[24] = audio;
        indices[24] = ElementCount::NoElementIndices;
        indices[24].input_idx = 11;
        static std::array<std::string, 16> harmonic_names;
        for (unsigned i = 0; i < 21; ++i)
        {
            indices[25 + i] = ElementCount::NoElementIndices;
            indices[25 + i].param_idx = 10 + i;
            if (i < 16)
            {
                harmonic_names[i] = "Harmonic " + std::to_string(i + 1);
                AltParamContinuous harmonic;
                harmonic.short_name = harmonic_names[i];
                harmonic.default_value = .5f + .5f / (i + 1);
                elements[25 + i] = harmonic;
            }
            else
            {
                AltParamChoiceLabeled option;
                static const char *names[]{"Ramp Mask", "Detune Range", "Detune Type", "Character",
                                           "Phase Reset"};
                option.short_name = names[i - 16];
                option.num_pos = i == 19 ? 3 : 2;
                option.default_value = i >= 19 ? 1 : 0;
                if (i == 16)
                    option.pos_names = {"Masked", "Unmasked"};
                if (i == 17)
                    option.pos_names = {"100 cents", "1200 cents"};
                if (i == 18)
                    option.pos_names = {"Relative", "Absolute"};
                if (i == 19)
                    option.pos_names = {"Warm", "Off", "Bright"};
                if (i == 20)
                    option.pos_names = {"Random", "Zero"};
                elements[25 + i] = option;
            }
        }
        DynamicTextDisplay selection;
        selection.x_mm = 5.5f;
        selection.y_mm = 38;
        selection.width_mm = 50;
        selection.height_mm = 7;
        selection.coords = Coords::TopLeft;
        selection.short_name = "Shape / Voices";
        selection.font = "Default_10";
        selection.color = Colors565::Orange;
        elements[46] = selection;
        indices[46] = ElementCount::NoElementIndices;
        indices[46].light_idx = 1;
        AltParamChoiceLabeled octave;
        octave.short_name = "Octave";
        octave.num_pos = 7;
        octave.default_value = 3;
        octave.pos_names = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
        elements[47] = octave;
        indices[47] = ElementCount::NoElementIndices;
        indices[47].param_idx = 31;
        JackInput pitch_mod;
        pitch_mod.x_mm = 23.48f;
        pitch_mod.y_mm = 55;
        pitch_mod.short_name = "Pitch Mod CV";
        pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
        elements[48] = pitch_mod;
        indices[48] = ElementCount::NoElementIndices;
        indices[48].input_idx = 12;
        // List the six Rack controls together, then options, jacks and displays.
        // Only presentation order changes: indices retain every saved patch ID.
        constexpr std::array<unsigned, 49> order{0,  2,  3,  6,  4,  5,  1,  7,  8,  9,  47, 25, 26,
                                                 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
                                                 40, 41, 42, 43, 44, 45, 10, 11, 48, 12, 13, 14, 15,
                                                 16, 17, 18, 19, 20, 24, 21, 22, 23, 46};
        std::array<unsigned, 49> positions{};
        for (unsigned i = 0; i < 49; ++i)
            positions[i] = i;
        for (unsigned target = 0; target < 49; ++target)
        {
            unsigned source = target;
            while (positions[source] != order[target])
                ++source;
            std::swap(elements[target], elements[source]);
            std::swap(indices[target], indices[source]);
            std::swap(positions[target], positions[source]);
        }
    }
    if constexpr (string)
    {
        KnobSnapped exciter;
        exciter.x_mm = 9.48f + (slots[1] % 4) * 14;
        exciter.y_mm = 55 + (slots[1] / 4) * 16;
        exciter.short_name = "Exciter Mode";
        exciter.default_value = 0;
        exciter.num_pos = 15;
        exciter.pos_names = {"Burst Noise",     "Burst Pink",        "Burst Sine",
                             "Burst Triangle",  "Burst Ramp",        "Burst Square",
                             "Burst Sweep",     "Constant Noise",    "Constant Pink",
                             "Constant Sine",   "Constant Triangle", "Constant Ramp",
                             "Constant Square", "Constant Sweep",    "Audio In"};
        exciter.image = "SurgeXTNative/components/rack-knob.png";
        elements[1] = exciter;
        JackInput audio;
        audio.x_mm = 30.5f;
        audio.y_mm = 42;
        audio.short_name = "Audio In";
        audio.image = "SurgeXTNative/components/rack-jack.png";
        elements[24] = audio;
        indices[24] = ElementCount::NoElementIndices;
        indices[24].input_idx = 11;
        static const char *names[]{"Internal Oversampling",
                                   "Interpolation",
                                   "Stiffness Filter",
                                   "String 1 Decay Polarity",
                                   "String 2 Decay Polarity",
                                   "Detune Range",
                                   "Detune Type",
                                   "Character",
                                   "Trigger Phase"};
        constexpr unsigned defaults[]{0, 2, 2, 0, 0, 0, 0, 1, 1};
        for (unsigned i = 0; i < 9; ++i)
        {
            AltParamChoiceLabeled option;
            option.short_name = names[i];
            option.num_pos = (i == 1 || i == 2 || i == 7) ? 3 : 2;
            option.default_value = defaults[i];
            if (i == 0)
                option.pos_names = {"1x", "2x"};
            if (i == 1)
                option.pos_names = {"Sample Hold", "Linear", "Sinc"};
            if (i == 2)
                option.pos_names = {"Fixed", "Keytrack", "Compensated"};
            if (i == 3 || i == 4)
                option.pos_names = {"Positive", "Bipolar"};
            if (i == 5)
                option.pos_names = {"100 cents", "1200 cents"};
            if (i == 6)
                option.pos_names = {"Relative", "Absolute"};
            if (i == 7)
                option.pos_names = {"Warm", "Off", "Bright"};
            if (i == 8)
                option.pos_names = {"Random", "Zero"};
            elements[25 + i] = option;
            indices[25 + i] = ElementCount::NoElementIndices;
            indices[25 + i].param_idx = 10 + i;
        }
        AltParamChoiceLabeled octave;
        octave.short_name = "Octave";
        octave.num_pos = 7;
        octave.default_value = 3;
        octave.pos_names = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
        elements[34] = octave;
        indices[34] = ElementCount::NoElementIndices;
        indices[34].param_idx = 19;
        JackInput pitch_mod;
        pitch_mod.x_mm = 8;
        pitch_mod.y_mm = 42;
        pitch_mod.short_name = "Pitch Mod CV";
        pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
        elements[35] = pitch_mod;
        indices[35] = ElementCount::NoElementIndices;
        indices[35].input_idx = 12;
    }
    if constexpr (twist)
    {
        KnobSnapped mode;
        mode.x_mm = 9.48f + (slots[1] % 4) * 14;
        mode.y_mm = 55 + (slots[1] / 4) * 16;
        mode.short_name = "Engine";
        mode.default_value = 0;
        mode.num_pos = 16;
        mode.pos_names = {
            "Waveforms",       "Waveshaper",     "2-Operator FM",  "Formant/PD",
            "Harmonic",        "Wavetable",      "Chords",         "Vowels/Speech",
            "Granular Cloud",  "Filtered Noise", "Particle Noise", "Inharmonic String",
            "Modal Resonator", "Analog Kick",    "Analog Snare",   "Analog Hi-Hat"};
        mode.image = "SurgeXTNative/components/rack-knob.png";
        elements[1] = mode;
        static const char *names[]{"Low-Pass Gate", "Aux Routing", "Character", "Trigger Phase"};
        constexpr unsigned defaults[]{0, 0, 1, 1};
        for (unsigned i = 0; i < 4; ++i)
        {
            AltParamChoiceLabeled option;
            option.short_name = names[i];
            option.num_pos = (i == 0 || i == 2) ? 3 : 2;
            option.default_value = defaults[i];
            if (i == 0)
                option.pos_names = {"Auto (trigger cable)", "Off", "On"};
            if (i == 1)
                option.pos_names = {"Mono Mix", "Stereo Pan"};
            if (i == 2)
                option.pos_names = {"Warm", "Off", "Bright"};
            if (i == 3)
                option.pos_names = {"Random", "Zero"};
            elements[24 + i] = option;
            indices[24 + i] = ElementCount::NoElementIndices;
            indices[24 + i].param_idx = 10 + i;
        }
        AltParamChoiceLabeled octave;
        octave.short_name = "Octave";
        octave.num_pos = 7;
        octave.default_value = 3;
        octave.pos_names = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
        elements[28] = octave;
        indices[28] = ElementCount::NoElementIndices;
        indices[28].param_idx = 14;
        JackInput pitch_mod;
        pitch_mod.x_mm = 30.5f;
        pitch_mod.y_mm = 42;
        pitch_mod.short_name = "Pitch Mod CV";
        pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
        elements[29] = pitch_mod;
        indices[29] = ElementCount::NoElementIndices;
        indices[29].input_idx = 11;
    }
    if constexpr (table)
    {
        static const char *names[]{"Morph Interpolation", "Detune Range",   "Detune Type",
                                   "Phase Reset",         "Enable Low Cut", "Enable High Cut"};
        for (unsigned i = 0; i < (window ? 6u : 4u); ++i)
        {
            AltParamChoiceLabeled option;
            option.short_name = names[i];
            option.num_pos = 2;
            option.default_value = (i == 0 || i == 3) ? 1 : 0;
            if (i == 0)
                option.pos_names = {"Stepped", "Smooth"};
            if (i == 1)
                option.pos_names = {"100 cents", "1200 cents"};
            if (i == 2)
                option.pos_names = {"Relative", "Absolute"};
            if (i == 3)
                option.pos_names = {"Random", "Zero"};
            if (i >= 4)
                option.pos_names = {"Off", "On"};
            elements[24 + i] = option;
            indices[24 + i] = ElementCount::NoElementIndices;
            indices[24 + i].param_idx = 10 + i;
        }
        constexpr unsigned extra = surge_mm::table_extra_param(Model), start = extra + 14;
        AltParamChoiceLabeled octave;
        octave.short_name = "Octave";
        octave.num_pos = 7;
        octave.default_value = 3;
        octave.pos_names = {"-3", "-2", "-1", "0", "+1", "+2", "+3"};
        elements[start] = octave;
        indices[start] = ElementCount::NoElementIndices;
        indices[start].param_idx = extra;
        AltParamChoiceLabeled character;
        character.short_name = "Character";
        character.num_pos = 3;
        character.default_value = 1;
        character.pos_names = {"Warm", "Off", "Bright"};
        elements[start + 1] = character;
        indices[start + 1] = ElementCount::NoElementIndices;
        indices[start + 1].param_idx = extra + 1;
        JackInput pitch_mod;
        pitch_mod.x_mm = 30.5f;
        pitch_mod.y_mm = 42;
        pitch_mod.short_name = "Pitch Mod CV";
        pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
        elements[start + 2] = pitch_mod;
        indices[start + 2] = ElementCount::NoElementIndices;
        indices[start + 2].input_idx = 11;
        if constexpr (window)
        {
            KnobSnapped shape;
            shape.x_mm = 9.48f + (slots[3] % 4) * 14;
            shape.y_mm = 55 + (slots[3] / 4) * 16;
            shape.short_name = "Window";
            shape.default_value = 0;
            shape.num_pos = 9;
            shape.pos_names = {"Triangle", "Cosine", "Blend 1", "Blend 2",  "Blend 3",
                               "Sawtooth", "Sine",   "Square",  "Rectangle"};
            shape.image = "SurgeXTNative/components/rack-knob.png";
            elements[3] = shape;
        }
    }
    if constexpr (surge_mm::standard_oscillator(Model))
    {
        // Keep the new controls beside the existing controls in the module
        // list. Stored parameter/jack IDs remain independent of list order.
        const auto old_elements = elements;
        const auto old_indices = indices;
        unsigned next = 0;
        auto append = [&](unsigned i) {
            elements[next] = old_elements[i];
            indices[next++] = old_indices[i];
        };
        for (unsigned i = 0; i < 10; ++i)
            append(i);
        for (unsigned i = 25; i < count; ++i)
            append(i);
        for (unsigned i = 10; i < 25; ++i)
            append(i);
    }
    if constexpr (twist)
    {
        const auto old_elements = elements;
        const auto old_indices = indices;
        unsigned next = 0;
        auto append = [&](unsigned i) {
            elements[next] = old_elements[i];
            indices[next++] = old_indices[i];
        };
        for (unsigned i = 0; i < 10; ++i)
            append(i);
        for (unsigned i = 24; i < 29; ++i)
            append(i);
        for (unsigned i = 10; i < 24; ++i)
            append(i);
        append(29);
    }
    if constexpr (string)
    {
        const auto old_elements = elements;
        const auto old_indices = indices;
        unsigned next = 0;
        auto append = [&](unsigned i) {
            elements[next] = old_elements[i];
            indices[next++] = old_indices[i];
        };
        for (unsigned i = 0; i < 10; ++i)
            append(i);
        for (unsigned i = 25; i < 35; ++i)
            append(i);
        for (unsigned i = 10; i < 25; ++i)
            append(i);
        append(35);
    }
    if constexpr (table)
    {
        const auto old_elements = elements;
        const auto old_indices = indices;
        unsigned next = 0;
        auto append = [&](unsigned i) {
            elements[next] = old_elements[i];
            indices[next++] = old_indices[i];
        };
        for (unsigned i = 0; i < 10; ++i)
            append(i);
        for (unsigned i = 24; i < count - 1; ++i)
            append(i);
        for (unsigned i = 10; i < 24; ++i)
            append(i);
        append(count - 1);
    }
    surge_mm::destroy(defaults);
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge oscillator with direct CV",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    using Processor = std::conditional_t<table, TableOscillator<Model>, OscillatorDirect<Model>>;
    register_module<Processor>("SurgeXTNative", slug, info, panel);
    if constexpr (table)
        register_context_menu("SurgeXTNative", slug,
                              {.get_items =
                                   [](CoreProcessor *module) {
                                       return static_cast<Processor *>(module)->menu_items();
                                   },
                               .on_change =
                                   [](CoreProcessor *module, unsigned index, float value) {
                                       static_cast<Processor *>(module)->menu_changed(index, value);
                                   }});
}
