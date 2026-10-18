// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "chow_processor.h"
#include "CoreModules/register_module.hh"
inline void register_chow()
{
    using namespace MetaModule;
    static std::array<Element, 13> elements;
    static std::array<ElementCount::Indices, 13> indices;
    for (unsigned id : {0u, 1u, 3u})
    {
        Knob k;
        k.x_mm = id == 0 ? 30.48f : id == 1 ? 37.48f : 51.48f;
        k.y_mm = id == 0 ? 34 : 68;
        k.short_name = surge_mm::chow_names[id];
        k.default_value = surge_mm::chow_defaults[id];
        k.image = id == 0 ? "SurgeXTNative/components/rack-big-knob.png"
                          : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_mult = 96;
            k.display_offset = -96;
            k.units = "dB";
        }
        if (id == 1)
        {
            k.display_mult = 19;
            k.display_offset = 1;
            k.units = ":1";
            k.display_precision = 3;
        }
        if (id == 3)
        {
            k.display_mult = 100;
            k.units = "%";
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    FlipSwitch flip;
    flip.x_mm = 23.48f;
    flip.y_mm = 68;
    flip.short_name = "Flip";
    flip.default_value = 0;
    flip.num_pos = 2;
    flip.pos_names = {"Positive", "Negative"};
    flip.frames = {"SurgeXTNative/components/ad-sum.png", "SurgeXTNative/components/ad-off.png"};
    elements[2] = flip;
    indices[2] = ElementCount::NoElementIndices;
    indices[2].param_idx = 2;
    AltParamChoiceLabeled poly;
    poly.short_name = "Polyphony";
    poly.num_pos = 2;
    poly.pos_names = {"Sum to stereo", "Polyphonic"};
    elements[4] = poly;
    indices[4] = ElementCount::NoElementIndices;
    indices[4].param_idx = 4;
    constexpr std::array<std::array<float, 2>, 6> xy{
        {{9.48f, 117}, {23.48f, 117}, {16.48f, 91}, {30.48f, 91}, {9.48f, 68}, {44.48f, 91}}};
    constexpr std::array<const char *, 6> names{"Audio Left", "Audio Right",     "Threshold CV",
                                                "Ratio CV",   "Flip Gate (>3V)", "Mix CV"};
    for (unsigned id = 0; id < 6; ++id)
    {
        JackInput j;
        j.x_mm = xy[id][0];
        j.y_mm = xy[id][1];
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[5 + id] = j;
        indices[5 + id] = ElementCount::NoElementIndices;
        indices[5 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[11 + id] = j;
        indices[11 + id] = ElementCount::NoElementIndices;
        indices[11 + id].output_idx = id;
    }
    ModuleInfoView info{.description = "Surge Chow with direct CV and Flip gate",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeChow>("SurgeXTNative", "Chow", info, "SurgeXTNative/Chow.png");
}
