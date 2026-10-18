// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "frequency_shifter_processor.h"
#include "CoreModules/register_module.hh"
inline void register_frequency_shifter()
{
    using namespace MetaModule;
    static std::array<Element, 18> elements;
    static std::array<ElementCount::Indices, 18> indices;
    constexpr std::array<std::array<float, 2>, 5> xy{
        {{16.48f, 30}, {44.48f, 30}, {23.48f, 74}, {37.48f, 74}, {51.48f, 74}}};
    for (unsigned id = 0; id < 5; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::frequency_shifter_names[id];
        k.default_value = surge_mm::frequency_shifter_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id < 2 ? "SurgeXTNative/components/rack-knob-14.png"
                         : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_mult = 200;
            k.display_offset = -100;
            k.units = "Hz";
        }
        if (id == 1)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        if (id == 2)
        {
            k.display_base = 256;
            k.display_mult = 1000.f / 256;
            k.display_precision = 4;
            k.units = "ms";
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    FlipSwitch extend;
    extend.x_mm = 30.48f;
    extend.y_mm = 30;
    extend.short_name = "Extend Frequency";
    extend.num_pos = 2;
    extend.pos_names = {"Normal", "100x displayed Hz"};
    extend.frames = {"SurgeXTNative/components/filter-off.png",
                     "SurgeXTNative/components/filter-on.png"};
    elements[5] = extend;
    indices[5] = ElementCount::NoElementIndices;
    indices[5].param_idx = 5;
    for (unsigned id = 6; id < 8; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 6 ? "Polyphony" : "Clock Format";
        a.num_pos = 2;
        if (id == 6)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else
            a.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<std::array<float, 2>, 8> jxy{{{9.48f, 117},
                                                       {23.48f, 117},
                                                       {16.48f, 52},
                                                       {44.48f, 52},
                                                       {23.48f, 95},
                                                       {37.48f, 95},
                                                       {51.48f, 95},
                                                       {9.48f, 74}}};
    constexpr std::array<const char *, 8> names{
        "Audio Left",    "Audio Right", "Left Shift CV", "Right Multiplier CV",
        "Delay Time CV", "Feedback CV", "Mix CV",        "Clock"};
    for (unsigned id = 0; id < 8; ++id)
    {
        JackInput j;
        j.x_mm = jxy[id][0];
        j.y_mm = jxy[id][1];
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[8 + id] = j;
        indices[8 + id] = ElementCount::NoElementIndices;
        indices[8 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].output_idx = id;
    }
    ModuleInfoView info{.description =
                            "Surge Frequency Shifter with five direct CV inputs and clock",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeFrequencyShifter>("SurgeXTNative", "FrequencyShifter", info,
                                           "SurgeXTNative/FrequencyShifter.png");
}
