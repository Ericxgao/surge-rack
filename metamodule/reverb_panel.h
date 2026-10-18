// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "reverb_processor.h"
#include "CoreModules/register_module.hh"
inline void register_reverb()
{
    using namespace MetaModule;
    static std::array<Element, 31> elements;
    static std::array<ElementCount::Indices, 31> indices;
    static std::array<std::string, 11> names;
    constexpr std::array<unsigned, 10> order{2, 3, 5, 6, 7, 8, 0, 4, 10, 9};
    for (unsigned slot = 0; slot < 10; ++slot)
    {
        unsigned id = order[slot];
        Knob k;
        k.x_mm = slot < 2 ? 16.48f + 28 * slot : 9.48f + 14 * ((slot - 2) % 4);
        k.y_mm = slot < 2 ? 23 : 43 + 17 * ((slot - 2) / 4);
        k.short_name = surge_mm::reverb_names[id];
        k.default_value = surge_mm::reverb_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = slot < 2 ? "SurgeXTNative/components/rack-big-knob.png"
                           : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_base = 8192;
            k.display_mult = 1000.f / 256;
            k.display_precision = 5;
            k.units = "ms";
        }
        if (id == 3)
        {
            k.display_base = 1024;
            k.display_mult = 1.f / 16;
            k.display_precision = 4;
            k.units = "s";
        }
        if (id == 10)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        if (id == 5 || id == 6 || id == 8)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75f;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 7)
        {
            k.display_mult = 96;
            k.display_offset = -48;
            k.units = "dB";
        }
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
    }
    for (unsigned slot = 0; slot < 5; ++slot)
    {
        unsigned id = slot == 0 ? 1 : 10 + slot;
        AltParamChoiceLabeled a;
        a.short_name = slot == 0   ? "Room Shape"
                       : slot == 1 ? "Low Cut Enable"
                       : slot == 2 ? "High Cut Enable"
                       : slot == 3 ? "Polyphony"
                                   : "Clock Format";
        a.num_pos = slot == 0 ? 4 : 2;
        a.default_value = slot == 1 || slot == 2 ? 1 : 0;
        if (slot == 0)
            a.pos_names = {"Type 1", "Type 2", "Type 3", "Type 4"};
        else if (slot < 3)
            a.pos_names = {"Off", "On"};
        else if (slot == 3)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else
            a.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        elements[10 + slot] = a;
        indices[10 + slot] = ElementCount::NoElementIndices;
        indices[10 + slot].param_idx = id;
    }
    constexpr std::array<unsigned, 11> cv_order{2, 3, 5, 6, 0, 4, 7, 8, 10, 9, 1};
    for (unsigned slot = 0; slot < 11; ++slot)
    {
        unsigned id = cv_order[slot];
        names[id] = std::string(surge_mm::reverb_names[id]) + " CV";
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 78 + 13 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[17 + slot] = j;
        indices[17 + slot] = ElementCount::NoElementIndices;
        indices[17 + slot].input_idx = id + 2;
    }
    for (unsigned id = 0; id < 3; ++id)
    {
        JackInput j;
        j.x_mm = id == 2 ? 51.48f : 9.48f + 14 * id;
        j.y_mm = id == 2 ? 104 : 117;
        j.short_name = id == 0 ? "Audio Left" : id == 1 ? "Audio Right" : "Clock";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 15 + id : 28;
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
        elements[29 + id] = j;
        indices[29 + id] = ElementCount::NoElementIndices;
        indices[29 + id].output_idx = id;
    }
    ModuleInfoView info{.description =
                            "Surge Reverb with eleven direct CV inputs and pre-delay clock",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeReverb>("SurgeXTNative", "Reverb", info, "SurgeXTNative/Reverb.png");
}
