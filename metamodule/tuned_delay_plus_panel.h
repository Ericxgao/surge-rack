// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tuned_delay_plus_processor.h"
#include "panel_controls.h"
#include "CoreModules/register_module.hh"
inline void register_tuned_delay_plus()
{
    using namespace MetaModule;
    static std::array<Element, 29> elements;
    static std::array<ElementCount::Indices, 29> indices;
    constexpr std::array<unsigned, 8> order{9, 1, 0, 2, 4, 5, 6, 3};
    constexpr std::array<float, 8> x{9.48, 23.48, 37.48, 51.48, 23.48, 37.48, 51.48, 51.48};
    constexpr std::array<float, 8> y{29, 29, 29, 29, 49, 49, 49, 69};
    static const char *names[]{"V/Oct Center", "Fine Left", "Fine Right",    "Feedback Level",
                               "LP Offset",    "HP Offset", "Filter Wet/Dry"};
    static std::array<std::string, 21> correction_names;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        unsigned id = order[slot];
        Knob k;
        k.x_mm = x[slot];
        k.y_mm = y[slot];
        k.default_value = surge_mm::tuned_plus_defaults[id];
        k.image = "SurgeXTNative/components/rack-knob.png";
        if (id != 9)
        {
            k.short_name = names[id];
            k.display_mult = 100;
            k.units = "%";
            if (id == 0)
            {
                k.display_mult = 10;
                k.display_offset = -5;
                k.units = "oct";
            }
            if (id == 1 || id == 2)
            {
                k.display_mult = 200;
                k.display_offset = -100;
                k.units = "cents";
            }
            if (id == 4 || id == 5)
            {
                k.display_mult = 110;
                k.display_offset = id == 4 ? -80 : -110;
                k.units = "semitones";
            }
            elements[slot] = k;
        }
        else
        {
            KnobSnapped c;
            c.x_mm = k.x_mm;
            c.y_mm = k.y_mm;
            c.default_value = k.default_value;
            c.image = k.image;
            c.short_name = "Sample Correction";
            c.num_pos = 21;
            for (unsigned i = 0; i < 21; ++i)
            {
                correction_names[i] = std::to_string(i);
                c.pos_names[i] = correction_names[i].c_str();
            }
            elements[slot] = c;
        }
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
    }
    for (unsigned slot = 0; slot < 4; ++slot)
    {
        unsigned id = slot < 2 ? 7 + slot : 8 + slot;
        AltParamChoiceLabeled a;
        a.short_name = slot == 0   ? "Lowpass Enabled"
                       : slot == 1 ? "Highpass Enabled"
                       : slot == 2 ? "Feedback Range"
                                   : "Input Clamp";
        a.num_pos = slot == 3 ? 3 : 2;
        a.default_value = slot == 3 ? 1 : 0;
        if (slot < 2)
            a.pos_names = {"Off", "On"};
        else if (slot == 2)
            a.pos_names = {"Compact (about 90-100%)", "Full (0-100%)"};
        else
            a.pos_names = {"+/-20 V", "+/-10 V", "+/-5 V"};
        elements[8 + slot] = a;
        indices[8 + slot] = ElementCount::NoElementIndices;
        indices[8 + slot].param_idx = id;
    }
    static const char *inputs[]{"Audio Left",          "Audio Right",    "Pitch V/Oct",
                                "Feedback Left",       "Feedback Right", "Noise Amplitude",
                                "Fine Left CV",        "Fine Right CV",  "Feedback Level CV",
                                "LP Offset CV",        "HP Offset CV",   "Filter Mix CV",
                                "Sample Correction CV"};
    for (unsigned id = 0; id < 13; ++id)
    {
        JackInput j;
        j.short_name = inputs[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        if (id < 2)
        {
            j.x_mm = 9.48f + 14 * id;
            j.y_mm = 117;
        }
        else if (id < 5)
        {
            j.x_mm = 9.48f + 14 * (id - 2);
            j.y_mm = 69;
        }
        else if (id == 5)
        {
            j.x_mm = 9.48f;
            j.y_mm = 49;
        }
        else
        {
            j.x_mm = 9.48f + 14 * ((id - 6) % 4);
            j.y_mm = 83 + 13 * ((id - 6) / 4);
        }
        elements[12 + id] = j;
        indices[12 + id] = ElementCount::NoElementIndices;
        indices[12 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id ? "Right" : "Left";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[25 + id] = j;
        indices[25 + id] = ElementCount::NoElementIndices;
        indices[25 + id].output_idx = id;
    }
    DynamicGraphicDisplay meter;
    meter.short_name = "Voice Levels";
    meter.x_mm = 9;
    meter.y_mm = 10;
    meter.width_mm = 43;
    meter.height_mm = 8;
    meter.coords = Coords::TopLeft;
    elements[27] = meter;
    indices[27] = ElementCount::NoElementIndices;
    indices[27].light_idx = 0;
    JackInput pitch_mod;
    pitch_mod.short_name = "Pitch Mod";
    pitch_mod.image = "SurgeXTNative/components/rack-jack.png";
    pitch_mod.x_mm = 51.48f;
    pitch_mod.y_mm = 96;
    elements[28] = pitch_mod;
    indices[28] = ElementCount::NoElementIndices;
    indices[28].input_idx = 13;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description =
                            "Surge tuned delay with external feedback, filters and excitation",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeTunedDelayPlus>("SurgeXTNative", "DelayLineByFreqExpanded", info,
                                         "SurgeXTNative/DelayLineByFreqExpanded.png");
}
