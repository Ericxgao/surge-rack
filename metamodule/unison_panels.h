// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "unison_processor.h"
#include "CoreModules/register_module.hh"
inline void register_unison_helper()
{
    using namespace MetaModule;
    static std::array<Element, 25> elements;
    static std::array<ElementCount::Indices, 25> indices;
    static const char *names[]{"Detune",    "Drift",           "Low Cut",
                               "High Cut",  "Unison Voices",   "Detune Range",
                               "Character", "Low Cut Enabled", "High Cut Enabled"};
    constexpr float defaults[]{.1f, 0, 0, 1, .25f, 0, .5f, 0, 0};
    for (unsigned id = 0; id < 9; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 4)
        {
            Knob knob;
            knob.short_name = names[id];
            knob.default_value = defaults[id];
            knob.x_mm = 37.48f + (id % 2) * 14;
            knob.y_mm = id < 2 ? 23 : 39;
            knob.image = "SurgeXTNative/components/rack-knob.png";
            knob.display_mult = 100;
            knob.units = "%";
            if (id >= 2)
            {
                knob.display_base = std::exp2(130.f / 12);
                knob.display_mult = 440.f / 32;
                knob.units = "Hz";
            }
            elements[id] = knob;
        }
        else if (id == 4)
        {
            KnobSnapped voices;
            voices.short_name = names[id];
            voices.x_mm = 16.48f;
            voices.y_mm = 31;
            voices.num_pos = 9;
            voices.default_value = .25f;
            voices.pos_names = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
            voices.image = "SurgeXTNative/components/rack-large-knob.png";
            elements[id] = voices;
        }
        else if (id == 6)
        {
            AltParamChoiceLabeled character;
            character.short_name = names[id];
            character.num_pos = 3;
            character.default_value = 1;
            character.pos_names = {"Warm", "Off", "Bright"};
            elements[id] = character;
        }
        else
        {
            FlipSwitch button;
            button.short_name = names[id];
            button.num_pos = 2;
            button.x_mm = id == 8 ? 58 : 44;
            button.y_mm = id == 5 ? 23 : 39;
            if (id == 5)
                button.pos_names = {"100 cents", "1200 cents"};
            else
                button.pos_names = {"Off", "On"};
            button.frames = {"SurgeXTNative/components/ad-off.png",
                             "SurgeXTNative/components/ad-sum.png"};
            elements[id] = button;
        }
    }
    static const char *input_names[]{"Pitch V/Oct", "VCO 1 Audio", "VCO 2 Audio",
                                     "VCO 3 Audio", "VCO 4 Audio", "Detune CV",
                                     "Drift CV",    "Low Cut CV",  "High Cut CV"};
    for (unsigned id = 0; id < 9; ++id)
    {
        JackInput jack;
        jack.short_name = input_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        jack.x_mm = id == 0 ? 9.48f : 9.48f + ((id - 1) % 4) * 14;
        jack.y_mm = id == 0 ? 114.5f : id < 5 ? 55 : 99.5f;
        elements[9 + id] = jack;
        indices[9 + id] = ElementCount::NoElementIndices;
        indices[9 + id].input_idx = id;
    }
    static const char *output_names[]{"Left",        "Right",       "VCO 1 Pitch",
                                      "VCO 2 Pitch", "VCO 3 Pitch", "VCO 4 Pitch"};
    for (unsigned id = 0; id < 6; ++id)
    {
        JackOutput jack;
        jack.short_name = output_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        jack.x_mm = id < 2 ? 37.48f + id * 14 : 9.48f + (id - 2) * 14;
        jack.y_mm = id < 2 ? 114.5f : 71;
        elements[18 + id] = jack;
        indices[18 + id] = ElementCount::NoElementIndices;
        indices[18 + id].output_idx = id;
    }
    DynamicTextDisplay status;
    status.short_name = "Voice routing";
    status.x_mm = 4.5f;
    status.y_mm = 80;
    status.width_mm = 52;
    status.height_mm = 9;
    status.coords = Coords::TopLeft;
    elements[24] = status;
    indices[24] = ElementCount::NoElementIndices;
    indices[24].light_idx = 0;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge polyphonic unison pitch distributor and stereo mixer",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeUnisonHelper>("SurgeXTNative", "UnisonHelper", info,
                                       "SurgeXTNative/UnisonHelper.png");
}
inline void register_unison_expander()
{
    using namespace MetaModule;
    static std::array<Element, 11> elements;
    static std::array<ElementCount::Indices, 11> indices;
    static std::array<std::string, 8> names;
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput jack;
        jack.short_name = id ? "CV 2" : "CV 1";
        jack.x_mm = 8.24f + id * 14;
        jack.y_mm = 45;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[id] = jack;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].input_idx = id;
    }
    for (unsigned id = 0; id < 8; ++id)
    {
        JackOutput jack;
        names[id] = "CV " + std::to_string(id / 4 + 1) + " to VCO " + std::to_string(id % 4 + 1);
        jack.short_name = names[id];
        jack.x_mm = 8.24f + (id / 4) * 14;
        jack.y_mm = 69.5f + (id % 4) * 15;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[2 + id] = jack;
        indices[2 + id] = ElementCount::NoElementIndices;
        indices[2 + id].output_idx = id;
    }
    DynamicTextDisplay status;
    status.short_name = "Expander link";
    status.x_mm = 3;
    status.y_mm = 18;
    status.width_mm = 24;
    status.height_mm = 19;
    status.coords = Coords::TopLeft;
    elements[10] = status;
    indices[10] = ElementCount::NoElementIndices;
    indices[10].light_idx = 0;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "CV duplication following a connected Surge Unison Helper",
                        .width_hp = 6,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeUnisonCVExpander>("SurgeXTNative", "UnisonHelperCVExpander", info,
                                           "SurgeXTNative/UnisonHelperCVExpander.png");
}
