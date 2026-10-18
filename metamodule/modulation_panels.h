// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "modulation_processor.h"
#include "CoreModules/register_module.hh"

inline void register_quad_ad()
{
    using namespace MetaModule;
    static std::array<Element, 49> elements;
    static std::array<ElementCount::Indices, 49> indices;
    static std::array<std::string, 49> names;
    static const char *groups[]{"Attack",      "Decay",    "Mode",         "Attack Curve",
                                "Decay Curve", "AD or AR", "Trigger Link", "Sum to Next"};
    SurgeQuadAD defaults;
    for (unsigned id = 0; id < 32; ++id)
    {
        unsigned group = id / 4, stage = id % 4;
        float x = 9.48f + stage * 14;
        names[id] = std::string(groups[group]) + " " + std::to_string(stage + 1);
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 8)
        {
            Knob knob;
            knob.x_mm = x;
            knob.y_mm = id < 4 ? 39 : 55;
            knob.short_name = names[id];
            knob.image = "SurgeXTNative/components/rack-knob.png";
            knob.default_value = defaults.get_param(id);
            knob.display_base = std::pow(2.f, surge_mm::ad_time_span);
            knob.display_mult = 1.f / 256;
            knob.units = "s";
            elements[id] = knob;
        }
        else
        {
            FlipSwitch button;
            button.x_mm = x;
            button.y_mm = 26;
            button.short_name = names[id];
            button.num_pos = 2;
            button.default_value = 0;
            if (group == 2)
            {
                button.pos_names = {"Digital", "Analog"};
                button.frames = {"SurgeXTNative/components/ad-digital.png",
                                 "SurgeXTNative/components/ad-analog.png"};
            }
            else if (group == 3 || group == 4)
            {
                button.x_mm = x + (group == 3 ? -4.5f : 4.5f);
                button.num_pos = 3;
                button.default_value = 1;
                button.pos_names = {"Faster", "Standard", "Slower"};
                button.frames = {"SurgeXTNative/components/ad-fast.png",
                                 "SurgeXTNative/components/ad-linear.png",
                                 "SurgeXTNative/components/ad-slow.png"};
            }
            else if (group == 5)
            {
                button.y_mm = 14;
                button.pos_names = {"AD", "AR"};
                button.frames = {"SurgeXTNative/components/ad-ad.png",
                                 "SurgeXTNative/components/ad-ar.png"};
            }
            else if (group == 6)
            {
                button.x_mm = x + 6.5f;
                button.y_mm = 71;
                button.num_pos = 3;
                button.default_value = 1;
                button.pos_names = {"Cycle", "Off", "Next"};
                button.frames = {"SurgeXTNative/components/ad-cycle.png",
                                 "SurgeXTNative/components/ad-off.png",
                                 "SurgeXTNative/components/ad-next.png"};
            }
            else
            {
                button.x_mm = x + 6.5f;
                button.y_mm = 114.5f;
                button.pos_names = {"Off", "On"};
                button.frames = {"SurgeXTNative/components/ad-off.png",
                                 "SurgeXTNative/components/ad-sum.png"};
            }
            elements[id] = button;
        }
    }
    AltParamChoiceLabeled retrigger;
    retrigger.short_name = "Retrigger from";
    retrigger.num_pos = 2;
    retrigger.pos_names = {"Current level", "Zero"};
    elements[32] = retrigger;
    indices[32] = ElementCount::NoElementIndices;
    indices[32].param_idx = 32;
    for (unsigned id = 0; id < 12; ++id)
    {
        JackInput jack;
        unsigned stage = id % 4;
        names[33 + id] = std::string(id < 4   ? "Trigger/Gate "
                                     : id < 8 ? "Attack CV "
                                              : "Decay CV ") +
                         std::to_string(stage + 1);
        jack.short_name = names[33 + id];
        jack.x_mm = 9.48f + stage * 14;
        jack.y_mm = id < 4 ? 71 : id < 8 ? 85 : 100;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[33 + id] = jack;
        indices[33 + id] = ElementCount::NoElementIndices;
        indices[33 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackOutput jack;
        names[45 + id] = "Envelope " + std::to_string(id + 1);
        jack.short_name = names[45 + id];
        jack.x_mm = 9.48f + id * 14;
        jack.y_mm = 114.5f;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[45 + id] = jack;
        indices[45 + id] = ElementCount::NoElementIndices;
        indices[45 + id].output_idx = id;
    }
    static constexpr std::array<ModuleInfoBase::BypassRoute, 4> bypass{
        {{0, 0}, {1, 1}, {2, 2}, {3, 3}}};
    ModuleInfoView info{.description = "Four Surge AD/AR envelopes with trigger and sum links",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices,
                        .bypass_routes = bypass};
    register_module<SurgeQuadAD>("SurgeXTNative", "QuadAD", info, "SurgeXTNative/QuadAD.png");
}

#include "matrix_profiles.h"
inline void register_mod_matrix()
{
    using namespace MetaModule;
    static std::array<Element, 61> elements;
    static std::array<ElementCount::Indices, 61> indices;
    static std::array<std::string, 52> names;
    for (unsigned id = 0; id < 40; ++id)
    {
        Knob knob;
        const unsigned target = id < 8 ? id : (id - 8) / 4;
        const unsigned source = id < 8 ? 0 : (id - 8) % 4;
        names[id] = id < 8 ? "Output " + std::to_string(target + 1) + " Offset"
                           : "Input " + std::to_string(source + 1) + " to Output " +
                                 std::to_string(target + 1);
        knob.short_name = names[id];
        knob.x_mm = 6 + target * 7;
        knob.y_mm = id < 8 ? 84 : 34 + source * 11;
        knob.image = "SurgeXTNative/components/rack-mini-knob.png";
        knob.default_value = .5f;
        knob.display_mult = id < 8 ? 20 : 200;
        knob.display_offset = id < 8 ? -10 : -100;
        knob.units = id < 8 ? "V" : "%";
        elements[id] = knob;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackInput jack;
        names[40 + id] = "Input " + std::to_string(id + 1);
        jack.short_name = names[40 + id];
        jack.x_mm = 9.48f + id * 14;
        jack.y_mm = 16;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[40 + id] = jack;
        indices[40 + id] = ElementCount::NoElementIndices;
        indices[40 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 8; ++id)
    {
        JackOutput jack;
        names[44 + id] = "Output " + std::to_string(id + 1);
        jack.short_name = names[44 + id];
        jack.x_mm = 9.48f + (id % 4) * 14;
        jack.y_mm = 100 + (id / 4) * 14.5f;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[44 + id] = jack;
        indices[44 + id] = ElementCount::NoElementIndices;
        indices[44 + id].output_idx = id;
    }
    AltParamChoiceLabeled voices;
    voices.short_name = "Minimum Voices";
    voices.num_pos = 4;
    voices.pos_names = {"1 (automatic)", "2", "3", "4"};
    elements[52] = voices;
    indices[52] = ElementCount::NoElementIndices;
    indices[52].param_idx = 40;
    static std::array<std::string, 8> curve_names;
    for (unsigned i = 0; i < 8; ++i)
    {
        curve_names[i] = "Output " + std::to_string(i + 1) + " Curve";
        AltParamChoiceLabeled curve;
        curve.short_name = curve_names[i];
        curve.num_pos = 3;
        curve.pos_names = {"Voltage (linear)", "Rack Alias waveform", "Rack unison"};
        elements[53 + i] = curve;
        indices[53 + i] = ElementCount::NoElementIndices;
        indices[53 + i].param_idx = 41 + i;
    }
    ModuleInfoView info{.description = "Surge four-input eight-output modulation matrix",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeModMatrix>("SurgeXTNative", "ModMatrix", info,
                                    "SurgeXTNative/ModMatrix.png");
    // Rack String Stiffness spans -100..100%, so full modulation depth is
    // 200%. The exporter stores that depth as 0.625 (0.5 V/V through this
    // matrix). Change only the readout; saved values and CV gain stay intact.
    static auto string_elements = elements;
    for (unsigned source = 0; source < 4; ++source)
    {
        auto &knob = std::get<Knob>(string_elements[8 + 7 * 4 + source]);
        knob.display_mult = 1600;
        knob.display_offset = -800;
    }
    // Nimbus depth units follow the Rack mode at export time. Only the
    // readout changes: the matrix still produces the same volts per volt.
    static std::array<std::array<Element, 61>, 5> nimbus_elements;
    for (unsigned mode = 0; mode < 5; ++mode)
    {
        nimbus_elements[mode] = elements;
        for (unsigned target = mode == 4 ? 0 : 2; target < (mode == 4 ? 4 : 8); ++target)
        {
            float span = 100;
            if (mode < 4)
            {
                if (target == 4)
                    span = 96;
                if (target == 5 && (mode == 0 || mode == 3))
                    span = 200;
                if (target == 6 && mode != 0)
                    span = 200;
            }
            for (unsigned source = 0; source < 4; ++source)
            {
                auto &knob = std::get<Knob>(nimbus_elements[mode][8 + target * 4 + source]);
                // Exported raw depth d is stored as 0.5 + d / 8.
                knob.display_mult = span * 8;
                knob.display_offset = -span * 4;
                knob.units = mode < 4 && target == 4 ? "semitones" : "%";
            }
        }
    }
    for (auto destination : surge_matrix_profiles)
    {
        const auto slug = std::string("ModMatrix") + destination;
        auto destination_info = info;
        if (std::string_view(destination) == "String")
            destination_info.elements = string_elements;
        const std::string_view name(destination);
        if (name.starts_with("Nimbus"))
        {
            unsigned mode = name.ends_with("Part2")    ? 4
                            : name == "NimbusPitch"    ? 1
                            : name == "NimbusLoop"     ? 2
                            : name == "NimbusSpectral" ? 3
                                                       : 0;
            destination_info.elements = nimbus_elements[mode];
        }
        register_module<SurgeModMatrix>("SurgeXTNative", slug.c_str(), destination_info,
                                        "SurgeXTNative/ModMatrix.png");
    }
}
