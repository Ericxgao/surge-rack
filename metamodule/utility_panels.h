// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "utility_processor.h"
#include "CoreModules/register_module.hh"
#include <string>

template <bool Sliders> void register_mixer()
{
    using namespace MetaModule;
    static std::array<Element, 38> elements;
    static std::array<ElementCount::Indices, 38> indices;
    static std::array<std::string, 21> names;
    static std::array<std::string, 14> input_names;
    static const char *control_names[]{
        "Input 1 Level",  "Input 2 Level",  "Input 3 Level", "Noise Level",
        "Ring 1x2 Level", "Ring 2x3 Level", "Noise Color",   "Gain"};
    SurgeMixer defaults;
    for (unsigned id = 0; id < 21; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 8)
        {
            names[id] = control_names[id];
            Knob knob;
            knob.x_mm = 9.48f + (id % 4) * 14;
            knob.y_mm = 38.5f + (id / 4) * 18;
            knob.short_name = names[id];
            knob.default_value = defaults.get_param(id);
            knob.image = "SurgeXTNative/components/rack-knob.png";
            knob.display_mult = id == 6 ? 200 : 100;
            knob.display_offset = id == 6 ? -100 : 0;
            knob.units = "%";
            if constexpr (Sliders)
            {
                Slider slider;
                static_cast<Pot &>(slider) = static_cast<const Pot &>(knob);
                slider.x_mm = 5.98f + id * 7;
                slider.y_mm = 46.5f;
                slider.image = "SurgeXTNative/components/rack-fader.png";
                slider.image_handle = "SurgeXTNative/components/rack-fader-handle.png";
                elements[id] = slider;
            }
            else
                elements[id] = knob;
        }
        else if (id < 20)
        {
            bool solo = id >= 14;
            unsigned source = (id - 8) % 6;
            names[id] = std::string(control_names[source]) + (solo ? " Solo" : " Mute");
            FlipSwitch button;
            button.short_name = names[id];
            button.x_mm = 8 + source * 9;
            button.y_mm = solo ? 21 : 14;
            button.num_pos = 2;
            button.default_value = defaults.get_param(id) > .5f;
            button.pos_names = {"Off", "On"};
            button.frames[0] = solo ? "SurgeXTNative/components/solo-off.png"
                                    : "SurgeXTNative/components/mute-off.png";
            button.frames[1] = solo ? "SurgeXTNative/components/solo-on.png"
                                    : "SurgeXTNative/components/mute-on.png";
            elements[id] = button;
        }
        else
        {
            AltParamChoiceLabeled meter;
            meter.short_name = "Meter voice";
            meter.num_pos = 4;
            meter.pos_names = {"1", "2", "3", "4"};
            elements[id] = meter;
        }
    }
    for (unsigned id = 0; id < 14; ++id)
    {
        JackInput jack;
        if (id < 6)
        {
            jack.x_mm = 9.48f + (id % 4) * 14;
            jack.y_mm = 99.5f + (id / 4) * 15;
            input_names[id] = "Input " + std::to_string(id / 2 + 1) + (id % 2 ? " Right" : " Left");
        }
        else
        {
            unsigned cv = id - 6;
            jack.x_mm = 9.48f + (cv % 4) * 14;
            jack.y_mm = 71.5f + (cv / 4) * 14;
            input_names[id] = std::string(control_names[cv]) + " CV";
        }
        jack.short_name = input_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[21 + id] = jack;
        indices[21 + id] = ElementCount::NoElementIndices;
        indices[21 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput jack;
        jack.x_mm = 37.48f + id * 14;
        jack.y_mm = 114.5f;
        jack.short_name = id ? "Right" : "Left";
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[35 + id] = jack;
        indices[35 + id] = ElementCount::NoElementIndices;
        indices[35 + id].output_idx = id;
    }
    DynamicGraphicDisplay meter;
    meter.x_mm = 5.5f;
    meter.y_mm = 26;
    meter.width_mm = 50;
    meter.height_mm = 4;
    meter.coords = Coords::TopLeft;
    meter.short_name = "VU";
    elements[37] = meter;
    indices[37] = ElementCount::NoElementIndices;
    indices[37].light_idx = 0;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge stereo mixer, noise and ring modulation",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeMixer>("SurgeXTNative", Sliders ? "MixerSlider" : "Mixer", info,
                                Sliders ? "SurgeXTNative/MixerSlider.png"
                                        : "SurgeXTNative/Mixer.png");
}

inline void register_digital_ring()
{
    using namespace MetaModule;
    static std::array<Element, 18> elements;
    static std::array<ElementCount::Indices, 18> indices;
    for (unsigned id = 0; id < 2; ++id)
    {
        KnobSnapped algorithm;
        algorithm.short_name = id ? "Algorithm 2" : "Algorithm 1";
        algorithm.x_mm = 8.24f;
        algorithm.y_mm = id ? 76 : 14;
        algorithm.image = "SurgeXTNative/components/rack-knob.png";
        algorithm.default_value = 0;
        algorithm.num_pos = 13;
        algorithm.pos_names = {"Ring",           "XOR43-0", "XOR43-1", "XOR43-2", "XOR43-3 Legacy",
                               "XOR43-4 Legacy", "XOR43-3", "XOR43-4", "XOR93-0", "XOR93-1",
                               "XOR93-2",        "XOR93-3", "XOR93-4"};
        elements[id] = algorithm;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    FlipSwitch link;
    link.short_name = "Link output 1 to A2";
    link.x_mm = 8.24f;
    link.y_mm = 65.5f;
    link.num_pos = 2;
    link.pos_names = {"Off", "On"};
    link.frames = {"SurgeXTNative/components/link-off.png", "SurgeXTNative/components/link-on.png"};
    elements[2] = link;
    indices[2] = ElementCount::NoElementIndices;
    indices[2].param_idx = 2;
    static const char *input_names[]{"A1 Left",        "A1 Right",       "B1 Left", "B1 Right",
                                     "A2 Left",        "A2 Right",       "B2 Left", "B2 Right",
                                     "Algorithm 1 CV", "Algorithm 2 CV", "Link CV"};
    for (unsigned id = 0; id < 11; ++id)
    {
        JackInput jack;
        if (id < 8)
        {
            jack.x_mm = 8.24f + (id % 2) * 14;
            jack.y_mm = (id < 4 ? 29.f : 90.f) + ((id % 4) / 2) * 14;
        }
        else
        {
            jack.x_mm = 22.24f;
            jack.y_mm = id == 8 ? 14 : id == 9 ? 76 : 65.5f;
        }
        jack.short_name = input_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[3 + id] = jack;
        indices[3 + id] = ElementCount::NoElementIndices;
        indices[3 + id].input_idx = id;
    }
    static const char *output_names[]{"1 Left", "1 Right", "2 Left", "2 Right"};
    for (unsigned id = 0; id < 4; ++id)
    {
        JackOutput jack;
        jack.x_mm = 8.24f + (id % 2) * 14;
        jack.y_mm = id < 2 ? 57 : 118;
        jack.short_name = output_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[14 + id] = jack;
        indices[14 + id] = ElementCount::NoElementIndices;
        indices[14 + id].output_idx = id;
    }
    static constexpr std::array<ModuleInfoBase::BypassRoute, 4> bypass{
        {{0, 0}, {1, 1}, {4, 2}, {5, 3}}};
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description =
                            "Two Surge oversampled digital ring modulators and combinators",
                        .width_hp = 6,
                        .elements = elements,
                        .indices = indices,
                        .bypass_routes = bypass};
    register_module<SurgeDigitalRing>("SurgeXTNative", "DigitalRingMod", info,
                                      "SurgeXTNative/DigitalRingMod.png");
}
