// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "filter_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
#include <string>
template <surge_mm::FilterModule Kind> void register_filter()
{
    using namespace MetaModule;
    constexpr bool vcf = Kind == surge_mm::FilterModule::VCF;
    constexpr unsigned parameters = vcf ? 7 : 9;
    static std::array<Element, parameters + 10> elements;
    static std::array<ElementCount::Indices, parameters + 10> indices;
    static std::array<std::string, 7> input_names;
    static const char *vcf_names[]{"Frequency", "Resonance", "Drive", "Mix", "Gain"};
    static const char *ws_names[]{"Drive", "Bias", "Gain", "Low Cut", "High Cut"};
    const auto names = vcf ? vcf_names : ws_names;
    const auto defaults = surge_mm::filter_defaults(Kind);
    const std::array<float, 5> x =
        vcf ? std::array<float, 5>{16.48f, 37.48f, 37.48f, 51.48f, 51.48f}
            : std::array<float, 5>{16.48f, 37.48f, 51.48f, 37.48f, 51.48f};
    const std::array<float, 5> y =
        vcf ? std::array<float, 5>{63, 55, 71, 55, 71} : std::array<float, 5>{63, 55, 55, 71, 71};
    for (unsigned id = 0; id < parameters; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 5)
        {
            Knob k;
            k.x_mm = x[id];
            k.y_mm = y[id];
            k.short_name = names[id];
            k.default_value = defaults[id];
            k.image = id == 0 ? "SurgeXTNative/components/rack-big-knob.png"
                              : "SurgeXTNative/components/rack-knob.png";
            k.display_mult = 100;
            k.units = "%";
            if (vcf && id == 0)
            {
                k.display_mult = 10;
                k.display_offset = -4;
                k.units = "oct";
            }
            if (!vcf && id == 0)
            {
                k.display_mult = 48;
                k.display_offset = -24;
                k.units = "dB";
            }
            if (!vcf && id == 1)
            {
                k.display_mult = 10;
                k.display_offset = -5;
                k.units = "V";
            }
            if (!vcf && id >= 3)
            {
                k.display_mult = 130;
                k.display_offset = 9;
                k.units = "MIDI";
            }
            elements[id] = k;
        }
        else if (id == 5 || (vcf && id == 6))
        {
            KnobSnapped k;
            k.x_mm = id == 5 ? 16.48f : 44.48f;
            k.y_mm = 42;
            k.short_name = id == 5 ? "Model" : "Subtype";
            k.num_pos = id == 5 ? surge_mm::filter_model_count(Kind) : 12;
            k.default_value = defaults[id];
            k.image = "SurgeXTNative/components/rack-small-knob.png";
            // The full model name is also in the panel display, including choices
            // beyond the SDK's fixed thirty-entry knob-label array.
            if (id == 5)
                for (unsigned n = 0; n < std::min<unsigned>(k.num_pos, k.pos_names.size()); ++n)
                    k.pos_names[n] = surge_mm::filter_model_name(Kind, n);
            elements[id] = k;
        }
        else
        {
            AltParamChoiceLabeled k;
            k.short_name = id == 6 ? "Enable Low Cut" : id == 7 ? "Enable High Cut" : "DC Block";
            k.num_pos = 2;
            k.default_value = unsigned(defaults[id]);
            k.pos_names = {"Off", "On"};
            elements[id] = k;
        }
    }
    for (unsigned id = 0; id < 7; ++id)
    {
        JackInput j;
        if (id < 2)
        {
            j.x_mm = 23.48f + 14 * id;
            j.y_mm = 102;
            input_names[id] = id ? "Right" : "Left";
        }
        else
        {
            unsigned c = id - 2;
            j.x_mm = 9.48f + 14 * (c % 4);
            j.y_mm = c < 4 ? 88 : 102;
            input_names[id] = std::string(names[c]) + " CV";
        }
        j.short_name = input_names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[parameters + id] = j;
        indices[parameters + id] = ElementCount::NoElementIndices;
        indices[parameters + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 116;
        j.short_name = id ? "Right" : "Left";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[parameters + 7 + id] = j;
        indices[parameters + 7 + id] = ElementCount::NoElementIndices;
        indices[parameters + 7 + id].output_idx = id;
    }
    DynamicTextDisplay display;
    display.x_mm = 6;
    display.y_mm = 12;
    display.width_mm = 49;
    display.height_mm = 22;
    display.coords = Coords::TopLeft;
    display.short_name = "Model Name";
    display.color = Colors565::Orange;
    display.wrap_mode = TextDisplay::WrapMode::Wrap;
    elements[parameters + 9] = display;
    indices[parameters + 9] = ElementCount::NoElementIndices;
    indices[parameters + 9].light_idx = 0;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description =
                            "Original Surge stereo processing with five direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeFilter<Kind>>("SurgeXTNative", vcf ? "VCF" : "Waveshaper", info,
                                       vcf ? "SurgeXTNative/VCF.png"
                                           : "SurgeXTNative/Waveshaper.png");
}
