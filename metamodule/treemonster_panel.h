// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "treemonster_processor.h"
#include "CoreModules/register_module.hh"
inline void register_treemonster()
{
    using namespace MetaModule;
    static std::array<Element, 25> elements;
    static std::array<ElementCount::Indices, 25> indices;
    static std::array<std::string, 8> names;
    constexpr std::array<std::array<float, 2>, 8> xy{{{9.48f, 45},
                                                      {23.48f, 45},
                                                      {37.48f, 45},
                                                      {51.48f, 45},
                                                      {16.48f, 27},
                                                      {44.48f, 27},
                                                      {37.48f, 62},
                                                      {51.48f, 62}}};
    for (unsigned id = 0; id < 8; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::treemonster_names[id];
        k.default_value = surge_mm::treemonster_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 4 || id == 5 ? "SurgeXTNative/components/rack-knob-14.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_mult = 96;
            k.display_offset = -96;
            k.units = "dB";
        }
        if (id == 2 || id == 3)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 4)
        {
            k.display_mult = 120;
            k.display_offset = -60;
            k.units = "st";
        }
        if (id == 6)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 8; id < 10; ++id)
    {
        FlipSwitch enabled;
        enabled.x_mm = 37.48f + 14 * (id - 8);
        enabled.y_mm = 53;
        enabled.short_name = id == 8 ? "Low Cut Enabled" : "High Cut Enabled";
        enabled.default_value = 1;
        enabled.num_pos = 2;
        enabled.pos_names = {"Off", "On"};
        enabled.frames = {"SurgeXTNative/components/filter-off.png",
                          "SurgeXTNative/components/filter-on.png"};
        elements[id] = enabled;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    AltParamChoiceLabeled poly;
    poly.short_name = "Polyphony";
    poly.num_pos = 2;
    poly.default_value = 1;
    poly.pos_names = {"Sum to stereo", "Polyphonic"};
    elements[10] = poly;
    indices[10] = ElementCount::NoElementIndices;
    indices[10].param_idx = 10;
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[11 + id] = j;
        indices[11 + id] = ElementCount::NoElementIndices;
        indices[11 + id].input_idx = id;
    }
    constexpr std::array<unsigned, 8> order{4, 5, 0, 1, 2, 3, 6, 7};
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto id = order[slot];
        names[id] = std::string(surge_mm::treemonster_names[id]) +
                    ((id == 2 || id == 3 || id == 4) ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 85 + 16 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[13 + id] = j;
        indices[13 + id] = ElementCount::NoElementIndices;
        indices[13 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackOutput j;
        j.x_mm = id < 2 ? 37.48f + 14 * id : 9.48f + 14 * (id - 2);
        j.y_mm = id < 2 ? 117 : 62;
        j.short_name = id == 0   ? "Left / Mono"
                       : id == 1 ? "Right"
                       : id == 2 ? "Detected Pitch V/Oct"
                                 : "Envelope";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[21 + id] = j;
        indices[21 + id] = ElementCount::NoElementIndices;
        indices[21 + id].output_idx = id;
    }
    ModuleInfoView info{
        .description = "Surge Tree Monster with eight direct CV inputs and pitch/envelope outputs",
        .width_hp = 12,
        .elements = elements,
        .indices = indices};
    register_module<SurgeTreeMonster>("SurgeXTNative", "TreeMonster", info,
                                      "SurgeXTNative/TreeMonster.png");
}
