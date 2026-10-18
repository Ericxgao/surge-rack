// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nimbus_processor.h"
#include "CoreModules/register_module.hh"
inline void register_nimbus()
{
    using namespace MetaModule;
    static std::array<Element, 29> elements;
    static std::array<ElementCount::Indices, 29> indices;
    static std::array<std::string, 9> names;
    constexpr std::array<unsigned, 9> order{0, 1, 2, 3, 6, 4, 5, 7, 8};
    constexpr std::array<std::array<float, 2>, 9> xy{{{9.48f, 24},
                                                      {23.48f, 24},
                                                      {37.48f, 24},
                                                      {51.48f, 24},
                                                      {51.48f, 44},
                                                      {9.48f, 64},
                                                      {23.48f, 64},
                                                      {37.48f, 64},
                                                      {51.48f, 64}}};
    for (unsigned slot = 0; slot < 9; ++slot)
    {
        auto cv = order[slot], id = surge_mm::nimbus_cv_params[cv];
        Knob k;
        k.x_mm = xy[slot][0];
        k.y_mm = xy[slot][1];
        k.short_name = surge_mm::nimbus_names[cv];
        k.default_value = surge_mm::nimbus_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = "SurgeXTNative/components/rack-knob.png";
        if (id == 5)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        if (id == 4)
        {
            k.display_mult = 96;
            k.display_offset = -48;
            k.units = "semitones";
        }
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
    }
    MomentaryButton freeze;
    freeze.x_mm = 37.48f;
    freeze.y_mm = 44;
    freeze.short_name = "Manual Freeze";
    freeze.image = "SurgeXTNative/components/knock-off.png";
    freeze.pressed_image = "SurgeXTNative/components/knock-on.png";
    elements[9] = freeze;
    indices[9] = ElementCount::NoElementIndices;
    indices[9].param_idx = 8;
    for (unsigned slot = 0; slot < 3; ++slot)
    {
        AltParamChoiceLabeled a;
        a.short_name = slot == 0 ? "Mode" : slot == 1 ? "Quality" : "Polyphony";
        a.num_pos = slot == 2 ? 2 : 4;
        if (slot == 0)
            a.pos_names = {"Granularizer", "Pitch Shifter / Time Stretch", "Looping Delay",
                           "Spectral Madness"};
        else if (slot == 1)
            a.pos_names = {"32k 16-bit Stereo", "32k 16-bit Mono", "16k 8-bit Stereo",
                           "16k 8-bit Mono"};
        else
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        elements[10 + slot] = a;
        indices[10 + slot] = ElementCount::NoElementIndices;
        indices[10 + slot].param_idx = slot == 2 ? 12 : slot;
    }
    for (unsigned slot = 0; slot < 13; ++slot)
    {
        JackInput j;
        j.image = "SurgeXTNative/components/rack-jack.png";
        if (slot < 2)
        {
            j.x_mm = 9.48f + 14 * slot;
            j.y_mm = 117;
            j.short_name = slot == 0 ? "Audio Left" : "Audio Right";
        }
        else if (slot < 11)
        {
            unsigned cv = slot - 2;
            j.x_mm = 9.48f + 14 * (cv % 4);
            j.y_mm = 78 + 13 * (cv / 4);
            names[cv] = std::string(surge_mm::nimbus_names[cv]) + " CV";
            j.short_name = names[cv];
        }
        else
        {
            j.x_mm = slot == 11 ? 23.48f : 9.48f;
            j.y_mm = 44;
            j.short_name = slot == 11 ? "Freeze Gate" : "Grain Trigger";
        }
        elements[13 + slot] = j;
        indices[13 + slot] = ElementCount::NoElementIndices;
        indices[13 + slot].input_idx = slot;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[26 + id] = j;
        indices[26 + id] = ElementCount::NoElementIndices;
        indices[26 + id].output_idx = id;
    }
    JackInput freeze_cv;
    freeze_cv.x_mm = 23.48f;
    freeze_cv.y_mm = 104;
    freeze_cv.short_name = "Freeze CV";
    freeze_cv.image = "SurgeXTNative/components/rack-jack.png";
    elements[28] = freeze_cv;
    indices[28] = ElementCount::NoElementIndices;
    indices[28].input_idx = 13;
    ModuleInfoView info{.description =
                            "Surge Nimbus with direct CV inputs, Freeze CV/gate and grain trigger",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeNimbus>("SurgeXTNative", "Nimbus", info, "SurgeXTNative/Nimbus.png");
}
