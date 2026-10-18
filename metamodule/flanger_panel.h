// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "flanger_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
inline void register_flanger()
{
    using namespace MetaModule;
    static std::array<Element, 29> elements;
    static std::array<ElementCount::Indices, 29> indices;
    static std::array<std::string, 11> names;
    constexpr std::array<std::array<float, 2>, 11> xy{{{9.48f, 11},
                                                       {51.48f, 11},
                                                       {16.48f, 27},
                                                       {44.48f, 27},
                                                       {23.48f, 43},
                                                       {37.48f, 43},
                                                       {51.48f, 43},
                                                       {9.48f, 59},
                                                       {23.48f, 59},
                                                       {37.48f, 59},
                                                       {51.48f, 59}}};
    for (unsigned id = 0; id < 11; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::flanger_names[id];
        k.default_value = surge_mm::flanger_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 2 || id == 3 ? "SurgeXTNative/components/rack-knob-12.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 2)
        {
            k.display_base = 65536;
            k.display_mult = 1.f / 128;
            k.display_precision = 3;
            k.units = "Hz";
        }
        if (id == 4)
        {
            k.display_mult = 3;
            k.display_offset = 1;
            k.units = "voices";
            k.display_precision = 3;
        }
        if (id == 5)
        {
            k.display_mult = 127;
            k.units = "MIDI";
            k.display_precision = 4;
        }
        if (id == 6)
        {
            k.display_mult = 12;
            k.units = "st";
            k.display_precision = 3;
        }
        if (id == 9)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        if (id == 10)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        elements[id] = k;
        if (id < 2)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = k.short_name;
            s.default_value = 0;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            if (id == 0)
            {
                s.num_pos = 4;
                s.pos_names = {"Dry + Combs", "Combs Only", "Dry + Arp Combs", "Arp Combs Only"};
            }
            else
            {
                s.num_pos = 6;
                s.pos_names = {"Sine", "Triangle", "Saw", "Noise", "Sample & Hold", "Square"};
            }
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 11; id < 13; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 11 ? "Polyphony" : "Clock Format";
        a.num_pos = 2;
        if (id == 11)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else
            a.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 11> order{2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1};
    for (unsigned slot = 0; slot < 11; ++slot)
    {
        auto id = order[slot];
        names[id] = std::string(surge_mm::flanger_names[id]) + (id == 5 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 76 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[15 + id] = j;
        indices[15 + id] = ElementCount::NoElementIndices;
        indices[15 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 3; ++id)
    {
        JackInput j;
        j.x_mm = id == 2 ? 9.48f : 9.48f + 14 * id;
        j.y_mm = id == 2 ? 43 : 117;
        j.short_name = id == 0 ? "Audio Left" : id == 1 ? "Audio Right" : "Clock";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 13 + id : 26;
        elements[element] = j;
        indices[element] = ElementCount::NoElementIndices;
        indices[element].input_idx = id < 2 ? id : 13;
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
    ModuleInfoView info{.description = "Surge Flanger with eleven direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeFlanger>("SurgeXTNative", "Flanger", info, "SurgeXTNative/Flanger.png");
}
