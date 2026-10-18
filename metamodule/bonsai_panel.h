// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bonsai_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
inline void register_bonsai()
{
    using namespace MetaModule;
    static std::array<Element, 29> elements;
    static std::array<ElementCount::Indices, 29> indices;
    static std::array<std::string, 11> names;
    constexpr std::array<std::array<float, 2>, 11> xy{{{9.48f, 59},
                                                       {9.48f, 43},
                                                       {23.48f, 43},
                                                       {9.48f, 11},
                                                       {51.48f, 11},
                                                       {16.48f, 27},
                                                       {37.48f, 43},
                                                       {51.48f, 43},
                                                       {44.48f, 27},
                                                       {23.48f, 59},
                                                       {51.48f, 59}}};
    for (unsigned id = 0; id < 11; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::bonsai_names[id];
        k.default_value = surge_mm::bonsai_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 5 || id == 8 ? "SurgeXTNative/components/rack-knob-12.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 0 || id == 1 || id == 7 || id == 9)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        elements[id] = k;
        if (id == 3 || id == 4)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = k.short_name;
            s.default_value = 0;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            if (id == 3)
            {
                s.num_pos = 2;
                s.pos_names = {"Tilt", "Pull Mids"};
            }
            else
            {
                s.num_pos = 4;
                s.pos_names = {"Gentle", "Normal", "Hard", "Harsh"};
            }
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 11; id < 14; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 11 ? "Polyphony" : id == 12 ? "Bass Routing" : "Bass Range";
        a.num_pos = 2;
        if (id == 11)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else if (id == 12)
            a.pos_names = {"Stereo", "Mono (Surge)"};
        else
            a.pos_names = {"Normal", "3x displayed dB"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[14 + id] = j;
        indices[14 + id] = ElementCount::NoElementIndices;
        indices[14 + id].input_idx = id;
    }
    constexpr std::array<unsigned, 11> order{5, 8, 3, 4, 1, 2, 6, 7, 0, 9, 10};
    for (unsigned slot = 0; slot < 11; ++slot)
    {
        auto id = order[slot];
        names[id] = std::string(surge_mm::bonsai_names[id]) + " CV";
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 76 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[27 + id] = j;
        indices[27 + id] = ElementCount::NoElementIndices;
        indices[27 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge Bonsai with eleven direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeBonsai>("SurgeXTNative", "Bonsai", info, "SurgeXTNative/Bonsai.png");
}
