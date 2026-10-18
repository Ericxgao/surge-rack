// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "neuron_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
inline void register_neuron()
{
    using namespace MetaModule;
    static std::array<Element, 32> elements;
    static std::array<ElementCount::Indices, 32> indices;
    static std::array<std::string, 12> names;
    constexpr std::array<std::array<float, 2>, 12> xy{{{9.48f, 27},
                                                       {23.48f, 27},
                                                       {37.48f, 27},
                                                       {51.48f, 27},
                                                       {51.48f, 43},
                                                       {9.48f, 59},
                                                       {23.48f, 59},
                                                       {9.48f, 11},
                                                       {23.48f, 43},
                                                       {37.48f, 43},
                                                       {37.48f, 59},
                                                       {51.48f, 59}}};
    for (unsigned id = 0; id < 12; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::neuron_names[id];
        k.default_value = surge_mm::neuron_defaults[id];
        k.image = "SurgeXTNative/components/rack-knob.png";
        k.display_mult = 100;
        k.units = "%";
        if (id == 0 || id == 10 || id == 11)
        {
            k.display_mult = 48;
            k.display_offset = -24;
            k.units = "dB";
        }
        if (id == 5)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 6)
        {
            k.display_mult = 192;
            k.display_offset = -96;
            k.units = "st";
        }
        if (id == 8)
        {
            k.display_base = 65536;
            k.display_mult = 1.f / 128;
            k.display_precision = 3;
            k.units = "Hz";
        }
        elements[id] = k;
        if (id == 7)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = "LFO Waveform";
            s.default_value = 0;
            s.num_pos = 6;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            s.pos_names = {"Sine",  "Triangle", "Saw", "Sample and Glide", "Sample and Hold",
                           "Square"};
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    FlipSwitch enabled;
    enabled.x_mm = 51.48f;
    enabled.y_mm = 11;
    enabled.short_name = "LFO Running (Off: Rate selects phase)";
    enabled.default_value = 1;
    enabled.num_pos = 2;
    enabled.pos_names = {"Phase control", "Running"};
    enabled.frames = {"SurgeXTNative/components/filter-off.png",
                      "SurgeXTNative/components/filter-on.png"};
    elements[12] = enabled;
    indices[12] = ElementCount::NoElementIndices;
    indices[12].param_idx = 12;
    for (unsigned id = 13; id < 15; ++id)
    {
        AltParamChoiceLabeled option;
        option.num_pos = 2;
        if (id == 13)
        {
            option.short_name = "Polyphony";
            option.pos_names = {"Sum to stereo", "Polyphonic"};
        }
        else
        {
            option.short_name = "Clock Format";
            option.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
        }
        elements[id] = option;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    constexpr std::array<unsigned, 12> order{0, 1, 2, 3, 8, 9, 4, 7, 5, 6, 10, 11};
    for (unsigned slot = 0; slot < 12; ++slot)
    {
        auto id = order[slot];
        names[id] =
            std::string(surge_mm::neuron_names[id]) + (id == 5 || id == 6 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 76 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[17 + id] = j;
        indices[17 + id] = ElementCount::NoElementIndices;
        indices[17 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[15 + id] = j;
        indices[15 + id] = ElementCount::NoElementIndices;
        indices[15 + id].input_idx = id;
    }
    JackInput clock;
    clock.x_mm = 9.48f;
    clock.y_mm = 43;
    clock.short_name = "Clock";
    clock.image = "SurgeXTNative/components/rack-jack.png";
    elements[29] = clock;
    indices[29] = ElementCount::NoElementIndices;
    indices[29].input_idx = 14;
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[30 + id] = j;
        indices[30 + id] = ElementCount::NoElementIndices;
        indices[30 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge Neuron with twelve direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeNeuron>("SurgeXTNative", "Neuron", info, "SurgeXTNative/Neuron.png");
}
