// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "reverb2_processor.h"
#include "CoreModules/register_module.hh"
inline void register_reverb2()
{
    using namespace MetaModule;
    static std::array<Element, 27> elements;
    static std::array<ElementCount::Indices, 27> indices;
    static std::array<std::string, 10> names;
    constexpr std::array<unsigned, 10> order{1, 2, 3, 5, 6, 7, 0, 4, 8, 9};
    for (unsigned slot = 0; slot < 10; ++slot)
    {
        unsigned id = order[slot];
        Knob k;
        k.x_mm = slot < 2 ? 16.48f + 28 * slot : 9.48f + 14 * ((slot - 2) % 4);
        k.y_mm = slot < 2 ? 23 : 43 + 17 * ((slot - 2) / 4);
        k.short_name = surge_mm::reverb2_names[id];
        k.default_value = surge_mm::reverb2_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = slot < 2 ? "SurgeXTNative/components/rack-big-knob.png"
                           : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_base = 512;
            k.display_mult = 1000.f / 256;
            k.display_precision = 5;
            k.units = "ms";
        }
        if (id == 1)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        if (id == 2)
        {
            k.display_base = 1024;
            k.display_mult = 1.f / 16;
            k.display_precision = 4;
            k.units = "s";
        }
        if (id == 8)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
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
    constexpr std::array<unsigned, 10> cv_order{1, 2, 3, 5, 0, 4, 6, 7, 8, 9};
    for (unsigned slot = 0; slot < 10; ++slot)
    {
        unsigned id = cv_order[slot];
        names[id] = std::string(surge_mm::reverb2_names[id]) + " CV";
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 78 + 13 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[14 + slot] = j;
        indices[14 + slot] = ElementCount::NoElementIndices;
        indices[14 + slot].input_idx = id + 2;
    }
    for (unsigned id = 0; id < 3; ++id)
    {
        JackInput j;
        j.x_mm = id == 2 ? 37.48f : 9.48f + 14 * id;
        j.y_mm = id == 2 ? 104 : 117;
        j.short_name = id == 0 ? "Audio Left" : id == 1 ? "Audio Right" : "Clock";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 12 + id : 24;
        elements[element] = j;
        indices[element] = ElementCount::NoElementIndices;
        indices[element].input_idx = id < 2 ? id : 12;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[25 + id] = j;
        indices[25 + id] = ElementCount::NoElementIndices;
        indices[25 + id].output_idx = id;
    }
    ModuleInfoView info{.description =
                            "Surge Reverb2 with ten direct CV inputs and pre-delay clock",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeReverb2>("SurgeXTNative", "Reverb2", info, "SurgeXTNative/Reverb2.png");
}
