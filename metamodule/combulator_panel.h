// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "combulator_processor.h"
#include "CoreModules/register_module.hh"
inline void register_combulator()
{
    using namespace MetaModule;
    static std::array<Element, 33> elements;
    static std::array<ElementCount::Indices, 33> indices;
    static std::array<std::string, 12> names;
    constexpr std::array<unsigned, 12> order{0, 1, 2, 3, 4, 6, 7, 8, 5, 9, 10, 11};
    for (unsigned slot = 0; slot < 12; ++slot)
    {
        unsigned id = order[slot];
        Knob k;
        k.x_mm = 9.48f + 14 * (slot % 4);
        k.y_mm = 25 + 17 * (slot / 4);
        k.short_name = surge_mm::combulator_names[id];
        k.default_value = surge_mm::combulator_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = "SurgeXTNative/components/rack-knob.png";
        if (id == 4 || id == 5 || id == 9 || id == 10)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
        names[id] =
            std::string(surge_mm::combulator_names[id]) + (id >= 1 && id <= 3 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = k.x_mm;
        j.y_mm = 77 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[18 + slot] = j;
        indices[18 + slot] = ElementCount::NoElementIndices;
        indices[18 + slot].input_idx = id + 2;
    }
    static const char *options[]{"Tone Filter", "Comb 2 Pitch Mode", "Comb 3 Pitch Mode",
                                 "Polyphony"};
    for (unsigned i = 0; i < 4; ++i)
    {
        AltParamChoiceLabeled a;
        a.short_name = options[i];
        a.num_pos = 2;
        a.default_value = i == 0 ? 1 : 0;
        if (i == 0)
            a.pos_names = {"Off", "On"};
        else if (i < 3)
            a.pos_names = {"Offset from center", "Independent frequency"};
        else
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        elements[12 + i] = a;
        indices[12 + i] = ElementCount::NoElementIndices;
        indices[12 + i].param_idx = 12 + i;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].input_idx = id;
        JackOutput o;
        o.x_mm = 37.48f + 14 * id;
        o.y_mm = 117;
        o.short_name = id == 0 ? "Left / Mono" : "Right";
        o.image = "SurgeXTNative/components/rack-jack.png";
        elements[30 + id] = o;
        indices[30 + id] = ElementCount::NoElementIndices;
        indices[30 + id].output_idx = id;
    }
    DynamicTextDisplay text;
    text.x_mm = 4;
    text.y_mm = 10;
    text.width_mm = 54;
    text.height_mm = 7;
    text.coords = Coords::TopLeft;
    text.short_name = "Comb Frequencies";
    text.font = "Default_10";
    text.color = Colors565::Orange;
    elements[32] = text;
    indices[32] = ElementCount::NoElementIndices;
    indices[32].light_idx = 0;
    ModuleInfoView info{.description = "Surge three-comb resonator with twelve direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeCombulator>("SurgeXTNative", "Combulator", info,
                                     "SurgeXTNative/Combulator.png");
}
