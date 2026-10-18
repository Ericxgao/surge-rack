// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "ringmod_processor.h"
#include "CoreModules/register_module.hh"
inline void register_ringmod()
{
    using namespace MetaModule;
    static std::array<Element, 29> elements;
    static std::array<ElementCount::Indices, 29> indices;
    static std::array<std::string, 29> shape_names;
    static std::array<std::string, 16> voice_names;
    static std::array<std::string, 9> cv_names;
    constexpr std::array<std::array<float, 2>, 9> xy{{{9.48f, 11},
                                                      {30.48f, 27},
                                                      {37.48f, 60},
                                                      {51.48f, 11},
                                                      {9.48f, 43},
                                                      {23.48f, 43},
                                                      {9.48f, 60},
                                                      {23.48f, 60},
                                                      {51.48f, 60}}};
    for (unsigned id = 0; id < 9; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::ringmod_names[id];
        k.default_value = surge_mm::ringmod_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 1 ? "SurgeXTNative/components/rack-knob-12.png"
                          : "SurgeXTNative/components/rack-knob.png";
        if (id == 1)
        {
            k.display_base = std::exp2(127.f / 12);
            k.display_mult = 8.175798915643707;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 2)
            k.units = "cents";
        if (id == 6 || id == 7)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_precision = 5;
            k.units = "Hz";
        }
        elements[id] = k;
        if (id == 0 || id == 3)
        {
            KnobSnapped s;
            s.default_value = 0;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = k.short_name;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            if (id == 0)
            {
                s.num_pos = 29;
                for (unsigned i = 0; i < 28; ++i)
                {
                    shape_names[i] = "Wave " + std::to_string(i + 1);
                    if (i < 8)
                        shape_names[i] += " (TX " + std::to_string(i + 1) + ")";
                    s.pos_names[i] = shape_names[i].c_str();
                }
                shape_names[28] = "Audio In";
                s.pos_names[28] = shape_names[28].c_str();
            }
            else
            {
                s.num_pos = 16;
                for (unsigned i = 0; i < 16; ++i)
                {
                    voice_names[i] = std::to_string(i + 1);
                    s.pos_names[i] = voice_names[i].c_str();
                }
            }
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 9; id < 11; ++id)
    {
        FlipSwitch enabled;
        enabled.x_mm = 9.48f + 14 * (id - 9);
        enabled.y_mm = 69;
        enabled.short_name = id == 9 ? "Low Cut Enabled" : "High Cut Enabled";
        enabled.default_value = 1;
        enabled.num_pos = 2;
        enabled.pos_names = {"Off", "On"};
        enabled.frames = {"SurgeXTNative/components/filter-off.png",
                          "SurgeXTNative/components/filter-on.png"};
        elements[id] = enabled;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    for (unsigned id = 11; id < 14; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 11 ? "Polyphony" : id == 12 ? "Detune Range" : "Detune Type";
        a.num_pos = 2;
        if (id == 11)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else if (id == 12)
            a.pos_names = {"Normal", "12x displayed cents"};
        else
            a.pos_names = {"Relative", "Absolute (Surge)"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 9> order{1, 2, 3, 0, 4, 5, 6, 7, 8};
    for (unsigned slot = 0; slot < 9; ++slot)
    {
        auto id = order[slot];
        cv_names[id] = std::string(surge_mm::ringmod_names[id]) +
                       ((id == 1 || id == 6 || id == 7) ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 79 + 13 * (slot / 4);
        j.short_name = cv_names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = id < 2 ? 117 : 43;
        j.short_name = id == 0   ? "Audio Left"
                       : id == 1 ? "Audio Right"
                       : id == 2 ? "Carrier Left / Mono"
                                 : "Carrier Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 14 + id : 23 + id;
        elements[element] = j;
        indices[element] = ElementCount::NoElementIndices;
        indices[element].input_idx = id < 2 ? id : 9 + id;
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
    ModuleInfoView info{.description =
                            "Surge Ring Modulator with nine direct CV inputs and stereo carrier",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeRingMod>("SurgeXTNative", "RingMod", info, "SurgeXTNative/RingMod.png");
}
