// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "phaser_processor.h"
#include "CoreModules/register_module.hh"
inline void register_phaser()
{
    using namespace MetaModule;
    static std::array<Element, 33> elements;
    static std::array<ElementCount::Indices, 33> indices;
    static std::array<std::string, 15> names;
    constexpr std::array<unsigned, 12> order{3, 4, 5, 11, 9, 0, 2, 1, 10, 8, 7, 6};
    for (unsigned slot = 0; slot < 12; ++slot)
    {
        auto id = order[slot];
        float x = 9.48f + 14 * (slot % 4), y = 25 + 14 * (slot / 4);
        Knob k;
        k.x_mm = x;
        k.y_mm = y;
        k.short_name = surge_mm::phaser_names[id];
        k.default_value = surge_mm::phaser_defaults[id];
        k.image = "SurgeXTNative/components/rack-knob.png";
        k.display_mult = 100;
        k.units = "%";
        if (id == 0 || id == 1 || id == 2 || id == 11)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        if (id == 3)
        {
            k.display_base = 65536;
            k.display_mult = 1.f / 128;
            k.display_precision = 3;
            k.units = "Hz";
        }
        if (id == 7)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        elements[id] = k;
        if (id == 8 || id == 10)
        {
            KnobSnapped s;
            s.x_mm = x;
            s.y_mm = y;
            s.short_name = k.short_name;
            s.default_value = k.default_value;
            s.image = k.image;
            if (id == 8)
            {
                s.num_pos = 16;
                s.pos_names = {"Legacy", "2",  "3",  "4",  "5",  "6",  "7",  "8",
                               "9",      "10", "11", "12", "13", "14", "15", "16"};
            }
            else
            {
                s.num_pos = 6;
                s.pos_names = {"Sine", "Triangle", "Saw", "Noise", "Sample & Hold", "Square"};
            }
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        JackInput jack;
        names[2 + id] = std::string(surge_mm::phaser_names[id]) + " CV";
        jack.short_name = names[2 + id];
        jack.x_mm = x;
        jack.y_mm = 70 + 14 * (slot / 4);
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[18 + id] = jack;
        indices[18 + id] = ElementCount::NoElementIndices;
        indices[18 + id].input_idx = 2 + id;
    }
    for (unsigned i = 12; i < 16; ++i)
    {
        AltParamChoiceLabeled option;
        option.short_name = i == 12   ? "Tone Filter"
                            : i == 13 ? "LFO Motion"
                            : i == 14 ? "Polyphony"
                                      : "Clock Format";
        option.num_pos = 2;
        option.default_value = i < 14 ? 1 : 0;
        if (i == 14)
            option.pos_names = {"Sum to stereo", "Polyphonic"};
        else if (i == 15)
            option.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        else
            option.pos_names = {"Off", "On"};
        elements[i] = option;
        indices[i] = ElementCount::NoElementIndices;
        indices[i].param_idx = i;
    }
    for (unsigned slot = 0; slot < 5; ++slot)
    {
        float x = 7 + 12 * slot;
        unsigned element = slot < 2 ? 16 + slot : slot == 2 ? 30 : slot == 3 ? 31 : 32;
        if (slot < 3)
        {
            JackInput j;
            j.x_mm = x;
            j.y_mm = 116;
            j.short_name = slot == 0 ? "Audio Left" : slot == 1 ? "Audio Right" : "Clock";
            j.image = "SurgeXTNative/components/rack-jack.png";
            elements[element] = j;
            indices[element] = ElementCount::NoElementIndices;
            indices[element].input_idx = slot < 2 ? slot : 14;
        }
        else
        {
            JackOutput j;
            j.x_mm = x;
            j.y_mm = 116;
            j.short_name = slot == 3 ? "Left / Mono" : "Right";
            j.image = "SurgeXTNative/components/rack-jack.png";
            elements[element] = j;
            indices[element] = ElementCount::NoElementIndices;
            indices[element].output_idx = slot - 3;
        }
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge Phaser with twelve dedicated CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgePhaser>("SurgeXTNative", "Phaser", info, "SurgeXTNative/Phaser.png");
}
