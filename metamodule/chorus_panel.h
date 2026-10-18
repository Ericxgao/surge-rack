// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "chorus_processor.h"
#include "CoreModules/register_module.hh"
inline void register_chorus()
{
    using namespace MetaModule;
    static std::array<Element, 25> elements;
    static std::array<ElementCount::Indices, 25> indices;
    static std::array<std::string, 8> names;
    constexpr std::array<std::array<float, 2>, 8> xy{{{37.48f, 45},
                                                      {16.48f, 27},
                                                      {44.48f, 27},
                                                      {51.48f, 45},
                                                      {9.48f, 62},
                                                      {23.48f, 62},
                                                      {51.48f, 62},
                                                      {37.48f, 62}}};
    for (unsigned id = 0; id < 8; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::chorus_names[id];
        k.default_value = surge_mm::chorus_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 1 || id == 2 ? "SurgeXTNative/components/rack-knob-14.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_base = 256;
            k.display_mult = 1000.f / 2048;
            k.display_precision = 3;
            k.units = "ms";
        }
        if (id == 1)
        {
            k.display_base = 65536;
            k.display_mult = 1.f / 128;
            k.display_precision = 3;
            k.units = "Hz";
        }
        if (id == 4 || id == 5)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 7)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 8; id < 10; ++id)
    {
        FlipSwitch enabled;
        enabled.x_mm = 9.48f + 14 * (id - 8);
        enabled.y_mm = 71;
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
    for (unsigned id = 10; id < 12; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 10 ? "Polyphony" : "Clock Format";
        a.num_pos = 2;
        if (id == 10)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else
            a.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 8> order{1, 2, 0, 3, 4, 5, 7, 6};
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto id = order[slot];
        names[id] =
            std::string(surge_mm::chorus_names[id]) + (id == 4 || id == 5 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 85 + 16 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[14 + id] = j;
        indices[14 + id] = ElementCount::NoElementIndices;
        indices[14 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 3; ++id)
    {
        JackInput j;
        j.x_mm = id == 2 ? 16.48f : 9.48f + 14 * id;
        j.y_mm = id == 2 ? 45 : 117;
        j.short_name = id == 0 ? "Audio Left" : id == 1 ? "Audio Right" : "Clock";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 12 + id : 22;
        elements[element] = j;
        indices[element] = ElementCount::NoElementIndices;
        indices[element].input_idx = id < 2 ? id : 10;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[23 + id] = j;
        indices[23 + id] = ElementCount::NoElementIndices;
        indices[23 + id].output_idx = id;
    }
    ModuleInfoView info{.description = "Surge Chorus with eight direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeChorus>("SurgeXTNative", "Chorus", info, "SurgeXTNative/Chorus.png");
}
