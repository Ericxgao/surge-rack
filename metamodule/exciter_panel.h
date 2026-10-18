// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "exciter_processor.h"
#include "CoreModules/register_module.hh"
inline void register_exciter()
{
    using namespace MetaModule;
    static std::array<Element, 15> elements;
    static std::array<ElementCount::Indices, 15> indices;
    static std::array<std::string, 5> names;
    constexpr std::array<float, 5> knobX{30.48f, 37.48f, 9.48f, 23.48f, 51.48f};
    for (unsigned id = 0; id < 5; ++id)
    {
        Knob k;
        k.x_mm = knobX[id];
        k.y_mm = id == 0 ? 30 : 68;
        k.short_name = surge_mm::exciter_names[id];
        k.default_value = .5f;
        k.image = id == 0 ? "SurgeXTNative/components/rack-big-knob.png"
                          : "SurgeXTNative/components/rack-knob.png";
        k.display_mult = 100;
        k.units = "%";
        if (id == 2 || id == 3)
        {
            k.display_base = 4;
            k.display_mult = id == 2 ? 5 : 50;
            k.display_precision = 4;
            k.units = "ms";
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    AltParamChoiceLabeled poly;
    poly.short_name = "Polyphony";
    poly.num_pos = 2;
    poly.pos_names = {"Sum to stereo", "Polyphonic"};
    elements[5] = poly;
    indices[5] = ElementCount::NoElementIndices;
    indices[5].param_idx = 5;
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[6 + id] = j;
        indices[6 + id] = ElementCount::NoElementIndices;
        indices[6 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 5; ++id)
    {
        names[id] = std::string(surge_mm::exciter_names[id]) + " CV";
        JackInput j;
        j.x_mm = knobX[id];
        j.y_mm = id == 0 ? 49 : 91;
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[8 + id] = j;
        indices[8 + id] = ElementCount::NoElementIndices;
        indices[8 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[13 + id] = j;
        indices[13 + id] = ElementCount::NoElementIndices;
        indices[13 + id].output_idx = id;
    }
    ModuleInfoView info{.description = "Surge Exciter with five direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeExciter>("SurgeXTNative", "Exciter", info, "SurgeXTNative/Exciter.png");
}
