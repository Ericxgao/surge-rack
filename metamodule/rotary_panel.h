// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "rotary_processor.h"
#include "CoreModules/register_module.hh"
inline void register_rotary()
{
    using namespace MetaModule;
    static std::array<Element, 24> elements;
    static std::array<ElementCount::Indices, 24> indices;
    static std::array<std::string, 8> names;
    constexpr std::array<std::array<float, 2>, 8> xy{{{16.48f, 31},
                                                      {9.48f, 63},
                                                      {23.48f, 63},
                                                      {44.48f, 31},
                                                      {44.48f, 47},
                                                      {30.48f, 15},
                                                      {37.48f, 63},
                                                      {51.48f, 63}}};
    for (unsigned id = 0; id < 8; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::rotary_names[id];
        k.default_value = surge_mm::rotary_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 0 || id == 3 ? "SurgeXTNative/components/rack-knob-12.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 0)
        {
            k.display_base = 65536;
            k.display_mult = 1.f / 128;
            k.display_precision = 3;
            k.units = "Hz";
        }
        if (id == 3)
            k.display_mult = 200;
        if (id == 6)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        elements[id] = k;
        if (id == 5)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = "Drive Model";
            s.default_value = 0;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            s.num_pos = 8;
            s.pos_names = {"Soft",    "Hard", "Asymmetric", "Sine",
                           "Digital", "OJD",  "Full Wave",  "Soft Fuzz"};
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    KnobSnapped enabled;
    enabled.x_mm = 30.48f;
    enabled.y_mm = 47;
    enabled.short_name = "Drive Enabled";
    enabled.image = "SurgeXTNative/components/rack-small-knob.png";
    enabled.default_value = 1;
    enabled.num_pos = 2;
    enabled.pos_names = {"Off", "On"};
    elements[8] = enabled;
    indices[8] = ElementCount::NoElementIndices;
    indices[8].param_idx = 8;
    for (unsigned id = 9; id < 11; ++id)
    {
        AltParamChoiceLabeled a;
        a.short_name = id == 9 ? "Polyphony" : "Clock Format";
        a.num_pos = 2;
        if (id == 9)
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        else
            a.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        elements[id] = a;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 8> order{0, 3, 4, 5, 1, 2, 6, 7};
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto id = order[slot];
        names[id] = std::string(surge_mm::rotary_names[id]) + " CV";
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 85 + 16 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[13 + id] = j;
        indices[13 + id] = ElementCount::NoElementIndices;
        indices[13 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 3; ++id)
    {
        JackInput j;
        j.x_mm = id == 2 ? 16.48f : 9.48f + 14 * id;
        j.y_mm = id == 2 ? 47 : 117;
        j.short_name = id == 0 ? "Audio Left" : id == 1 ? "Audio Right" : "Clock";
        j.image = "SurgeXTNative/components/rack-jack.png";
        unsigned element = id < 2 ? 11 + id : 21;
        elements[element] = j;
        indices[element] = ElementCount::NoElementIndices;
        indices[element].input_idx = id < 2 ? id : 10;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[22 + id] = j;
        indices[22 + id] = ElementCount::NoElementIndices;
        indices[22 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge Rotary Speaker with eight direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeRotary>("SurgeXTNative", "RotarySpeaker", info,
                                 "SurgeXTNative/RotarySpeaker.png");
}
