// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "distortion_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
inline void register_distortion()
{
    using namespace MetaModule;
    static std::array<Element, 34> elements;
    static std::array<ElementCount::Indices, 34> indices;
    static std::array<std::string, 12> names;
    constexpr std::array<std::array<float, 2>, 12> xy{{{9.48f, 43},
                                                       {23.48f, 43},
                                                       {37.48f, 43},
                                                       {51.48f, 43},
                                                       {16.48f, 27},
                                                       {44.48f, 27},
                                                       {9.48f, 59},
                                                       {23.48f, 59},
                                                       {37.48f, 59},
                                                       {51.48f, 59},
                                                       {51.48f, 11},
                                                       {9.48f, 11}}};
    for (unsigned id = 0; id < 12; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::distortion_names[id];
        k.default_value = surge_mm::distortion_defaults[id];
        k.image = id == 4 || id == 5 ? "SurgeXTNative/components/rack-knob-12.png"
                  : id == 10         ? "SurgeXTNative/components/rack-small-knob.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        k.display_mult = surge_mm::distortion_range[id];
        k.display_offset = surge_mm::distortion_minimum[id];
        k.display_precision = 4;
        k.units = "dB";
        if (id == 1 || id == 3 || id == 7 || id == 9)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_offset = 0;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 2 || id == 8)
            k.units = "oct";
        if (id == 5)
        {
            k.display_mult = 200;
            k.display_offset = -100;
            k.units = "%";
        }
        elements[id] = k;
        if (id == 11)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = "Model";
            s.default_value = 0;
            s.num_pos = 8;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            s.pos_names = {"Soft",    "Hard", "Asymmetric", "Sine",
                           "Digital", "OJD",  "Full Wave",  "Soft Fuzz"};
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        FlipSwitch enabled;
        enabled.x_mm = 58;
        enabled.y_mm = 49 + 16 * i;
        enabled.short_name = i == 0 ? "Pre High Cut Enabled" : "Post High Cut Enabled";
        enabled.default_value = 1;
        enabled.num_pos = 2;
        enabled.pos_names = {"Off", "On"};
        enabled.frames = {"SurgeXTNative/components/filter-off.png",
                          "SurgeXTNative/components/filter-on.png"};
        elements[12 + i] = enabled;
        indices[12 + i] = ElementCount::NoElementIndices;
        indices[12 + i].param_idx = 12 + i;
    }
    for (unsigned id = 14; id < 18; ++id)
    {
        AltParamChoiceLabeled option;
        option.num_pos = 2;
        if (id == 14)
        {
            option.short_name = "Polyphony";
            option.pos_names = {"Sum to stereo", "Polyphonic"};
        }
        if (id == 15)
        {
            option.short_name = "Pre Gain Range";
            option.pos_names = {"Normal", "3x displayed dB"};
        }
        if (id == 16)
        {
            option.short_name = "Post Gain Range";
            option.pos_names = {"Normal", "3x displayed dB"};
        }
        if (id == 17)
        {
            option.short_name = "Drive Range";
            option.pos_names = {"Normal", "5x displayed dB"};
        }
        elements[id] = option;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 12> order{4, 5, 11, 10, 0, 1, 2, 3, 6, 7, 8, 9};
    for (unsigned slot = 0; slot < 12; ++slot)
    {
        auto id = order[slot];
        names[id] = std::string(surge_mm::distortion_names[id]) +
                    (id == 1 || id == 3 || id == 7 || id == 9 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 76 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[20 + id] = j;
        indices[20 + id] = ElementCount::NoElementIndices;
        indices[20 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[18 + id] = j;
        indices[18 + id] = ElementCount::NoElementIndices;
        indices[18 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[32 + id] = j;
        indices[32 + id] = ElementCount::NoElementIndices;
        indices[32 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge Distortion with twelve direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeDistortion>("SurgeXTNative", "Distortion", info,
                                     "SurgeXTNative/Distortion.png");
}
