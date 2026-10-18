// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "spring_processor.h"
#include "CoreModules/register_module.hh"
inline void register_spring()
{
    using namespace MetaModule;
    static std::array<Element, 22> elements;
    static std::array<ElementCount::Indices, 22> indices;
    constexpr std::array<unsigned, 7> order{0, 1, 2, 4, 5, 3, 7};
    constexpr std::array<std::array<float, 2>, 7> xy{{{16.48f, 25},
                                                      {44.48f, 25},
                                                      {44.48f, 50},
                                                      {9.48f, 70},
                                                      {23.48f, 70},
                                                      {37.48f, 70},
                                                      {51.48f, 70}}};
    for (unsigned slot = 0; slot < 7; ++slot)
    {
        unsigned id = order[slot];
        Knob k;
        k.x_mm = xy[slot][0];
        k.y_mm = xy[slot][1];
        k.short_name = surge_mm::spring_names[id];
        k.default_value = surge_mm::spring_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id < 2 ? "SurgeXTNative/components/rack-big-knob.png"
                         : "SurgeXTNative/components/rack-knob.png";
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
    }
    MomentaryButton knock;
    knock.x_mm = 23.48f;
    knock.y_mm = 50;
    knock.short_name = "Knock";
    knock.image = "SurgeXTNative/components/knock-off.png";
    knock.pressed_image = "SurgeXTNative/components/knock-on.png";
    elements[7] = knock;
    indices[7] = ElementCount::NoElementIndices;
    indices[7].param_idx = 6;
    AltParamChoiceLabeled poly;
    poly.short_name = "Polyphony";
    poly.num_pos = 2;
    poly.pos_names = {"Sum to stereo", "Polyphonic"};
    elements[8] = poly;
    indices[8] = ElementCount::NoElementIndices;
    indices[8].param_idx = 8;
    constexpr std::array<std::array<float, 2>, 10> jxy{{{9.48f, 117},
                                                        {23.48f, 117},
                                                        {9.48f, 88},
                                                        {23.48f, 88},
                                                        {37.48f, 88},
                                                        {37.48f, 102},
                                                        {9.48f, 102},
                                                        {23.48f, 102},
                                                        {51.48f, 88},
                                                        {9.48f, 50}}};
    constexpr std::array<const char *, 10> names{
        "Audio Left",    "Audio Right", "Size CV",  "Decay CV", "Reflections CV",
        "HF Damping CV", "Spin CV",     "Chaos CV", "Mix CV",   "Knock Trigger"};
    for (unsigned id = 0; id < 10; ++id)
    {
        JackInput j;
        j.x_mm = jxy[id][0];
        j.y_mm = jxy[id][1];
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[9 + id] = j;
        indices[9 + id] = ElementCount::NoElementIndices;
        indices[9 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[19 + id] = j;
        indices[19 + id] = ElementCount::NoElementIndices;
        indices[19 + id].output_idx = id;
    }
    JackInput knock_cv;
    knock_cv.x_mm = 51.48f;
    knock_cv.y_mm = 102;
    knock_cv.short_name = "Knock CV";
    knock_cv.image = "SurgeXTNative/components/rack-jack.png";
    elements[21] = knock_cv;
    indices[21] = ElementCount::NoElementIndices;
    indices[21].input_idx = 10;
    ModuleInfoView info{.description =
                            "Surge Spring Reverb with direct CV inputs, Knock CV and trigger",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeSpringReverb>("SurgeXTNative", "SpringReverb", info,
                                       "SurgeXTNative/SpringReverb.png");
}
