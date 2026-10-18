// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vocoder_processor.h"
#include "CoreModules/register_module.hh"
inline void register_vocoder()
{
    using namespace MetaModule;
    static std::array<Element, 27> elements;
    static std::array<ElementCount::Indices, 27> indices;
    static std::array<std::string, 9> names;
    constexpr std::array<std::array<float, 2>, 9> xy{{{9.48f, 25},
                                                      {23.48f, 25},
                                                      {9.48f, 59},
                                                      {23.48f, 59},
                                                      {37.48f, 25},
                                                      {51.48f, 25},
                                                      {9.48f, 42},
                                                      {23.48f, 42},
                                                      {51.48f, 59}}};
    for (unsigned id = 0; id < 9; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::vocoder_names[id];
        k.default_value = surge_mm::vocoder_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = "SurgeXTNative/components/rack-knob.png";
        if (id < 2)
        {
            k.display_mult = 96;
            k.display_offset = id == 0 ? -48 : -96;
            k.units = "dB";
        }
        if (id == 3 || id == 6 || id == 7)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        if (id == 4 || id == 5)
        {
            k.display_base = std::exp2((id == 4 ? 72.f : 60.f) / 12);
            k.display_mult = id == 4 ? 55 : 440;
            k.display_precision = 5;
            k.units = "Hz";
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 9; id < 12; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 9 ? "Bands" : id == 10 ? "Modulator Input" : "Polyphony";
        a.default_value = id == 9 ? 4 : 0;
        if (id == 9)
        {
            a.num_pos = 5;
            a.pos_names = {"4", "8", "12", "16", "20"};
        }
        else if (id == 10)
        {
            a.num_pos = 4;
            a.pos_names = {"L + R", "Left", "Right", "Stereo"};
        }
        else
        {
            a.num_pos = 2;
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        }
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackInput j;
        j.x_mm = id < 2 ? 9.48f + 14 * id : 37.48f + 14 * (id - 2);
        j.y_mm = id < 2 ? 117 : 42;
        j.short_name = id == 0   ? "Carrier Left / Mono"
                       : id == 1 ? "Carrier Right"
                       : id == 2 ? "Modulator Left / Mono"
                                 : "Modulator Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[12 + id] = j;
        indices[12 + id] = ElementCount::NoElementIndices;
        indices[12 + id].input_idx = id;
    }
    constexpr std::array<unsigned, 9> order{0, 1, 4, 5, 6, 7, 2, 3, 8};
    for (unsigned slot = 0; slot < 9; ++slot)
    {
        auto id = order[slot];
        names[id] =
            std::string(surge_mm::vocoder_names[id]) + ((id == 4 || id == 5) ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 77 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].input_idx = 4 + id;
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
                            "Surge Vocoder with stereo modulator and nine direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeVocoder>("SurgeXTNative", "Vocoder", info, "SurgeXTNative/Vocoder.png");
}
