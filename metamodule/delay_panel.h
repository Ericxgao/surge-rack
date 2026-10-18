// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "delay_processor.h"
#include "CoreModules/register_module.hh"
inline void register_delay()
{
    using namespace MetaModule;
    static std::array<Element, 28> elements;
    static std::array<ElementCount::Indices, 28> indices;
    static const char *names[]{"Left Delay",       "Right Delay", "Time Tweak", "Feedback",
                               "Crossfeed",        "Low Cut",     "High Cut",   "Modulation Rate",
                               "Modulation Depth", "Mix"};
    constexpr std::array<float, 10> x{13.48, 47.48, 30.48, 9.48,  23.48,
                                      37.48, 51.48, 23.48, 37.48, 51.48};
    constexpr std::array<float, 10> y{28, 28, 30, 47, 47, 47, 47, 65, 65, 65};
    for (unsigned id = 0; id < 10; ++id)
    {
        Knob k;
        k.x_mm = x[id];
        k.y_mm = y[id];
        k.short_name = names[id];
        k.default_value = surge_mm::delay_defaults[id];
        k.image = "SurgeXTNative/components/rack-knob.png";
        k.display_mult = 100;
        k.units = "%";
        if (id < 2)
        {
            k.min_value = surge_mm::delay_time_min;
            k.max_value = surge_mm::delay_time_min + surge_mm::delay_time_range;
            k.display_base = 2;
            k.display_mult = 1;
            k.units = " s";
            k.display_precision = 4;
        }
        if (id == 2)
        {
            k.display_mult = 4;
            k.display_offset = -2;
        }
        if (id == 5 || id == 6)
        {
            k.min_value = -5;
            k.max_value = 70.f / 12;
            k.display_base = 2;
            k.display_mult = 440;
            k.units = " Hz";
            k.display_precision = 4;
        }
        elements[id] = k;
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    AltParamChoiceLabeled clip;
    clip.short_name = "Delay Line Clipping";
    clip.num_pos = 4;
    clip.default_value = 3;
    clip.pos_names = {"Transparent", "Soft +/-5 V", "Soft +/-10 V", "Hard +/-10 V"};
    elements[10] = clip;
    indices[10] = ElementCount::NoElementIndices;
    indices[10].param_idx = 10;
    AltParamChoiceLabeled clock;
    clock.short_name = "Clock Format";
    clock.num_pos = 2;
    clock.pos_names = {"Quarter-note pulses", "BPM V/Oct (0 V = 120)"};
    elements[11] = clock;
    indices[11] = ElementCount::NoElementIndices;
    indices[11].param_idx = 11;
    static const char *input_names[]{"Audio Left",   "Audio Right",        "Clock/BPM",
                                     "Left Time CV", "Right Time CV",      "Time Tweak CV",
                                     "Feedback CV",  "Crossfeed CV",       "Low Cut CV",
                                     "High Cut CV",  "Modulation Rate CV", "Modulation Depth CV",
                                     "Mix CV"};
    for (unsigned id = 0; id < 13; ++id)
    {
        JackInput j;
        j.short_name = input_names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        if (id < 2)
        {
            j.x_mm = 9.48 + 14 * id;
            j.y_mm = 117;
        }
        else if (id == 2)
        {
            j.x_mm = 9.48;
            j.y_mm = 65;
        }
        else
        {
            j.x_mm = 9.48 + 14 * ((id - 3) % 4);
            j.y_mm = 80 + 12 * ((id - 3) / 4);
        }
        elements[12 + id] = j;
        indices[12 + id] = ElementCount::NoElementIndices;
        indices[12 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48 + 14 * id;
        j.y_mm = 117;
        j.short_name = id ? "Right" : "Left";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[25 + id] = j;
        indices[25 + id] = ElementCount::NoElementIndices;
        indices[25 + id].output_idx = id;
    }
    DynamicTextDisplay display;
    display.x_mm = 3;
    display.y_mm = 10;
    display.width_mm = 55;
    display.height_mm = 6;
    display.coords = Coords::TopLeft;
    display.short_name = "Delay Times";
    display.font = "Default_10";
    display.color = Colors565::Orange;
    elements[27] = display;
    indices[27] = ElementCount::NoElementIndices;
    indices[27].light_idx = 0;
    ModuleInfoView info{.description = "Surge stereo delay with cross-feedback and modulation",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeDelay>("SurgeXTNative", "Delay", info, "SurgeXTNative/Delay.png");
}
