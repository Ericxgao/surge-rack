// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lfo_processor.h"
#include "panel_controls.h"
#include "CoreModules/register_module.hh"
inline void register_lfo()
{
    using namespace MetaModule;
    static std::array<Element, 79> elements;
    static std::array<ElementCount::Indices, 79> indices;
    static std::array<std::string, 79> names;
    static const char *primary[]{"Rate",           "Phase / Shuffle", "Deform", "Amplitude",
                                 "Envelope Delay", "Attack",          "Hold",   "Decay",
                                 "Sustain",        "Release"};
    for (unsigned id = 0; id < 58; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 4)
        {
            Knob k;
            k.short_name = primary[id];
            k.x_mm = 9.48 + 14 * id;
            k.y_mm = 38;
            k.default_value = surge_mm::lfo_defaults[id];
            k.image = "SurgeXTNative/components/rack-knob.png";
            k.display_mult = 100;
            k.units = "%";
            if (id == 0)
            {
                k.min_value = -7;
                k.max_value = 9;
                k.display_base = 2;
                k.display_mult = 1;
                k.units = " Hz";
                k.display_precision = 4;
            }
            if (id == 2)
            {
                k.display_mult = 200;
                k.display_offset = -100;
            }
            elements[id] = k;
        }
        else if (id < 10)
        {
            Slider s;
            s.short_name = primary[id];
            s.x_mm = 61.5 + 7 * (id - 4);
            s.y_mm = 43;
            s.default_value = surge_mm::lfo_defaults[id];
            s.image = "SurgeXTNative/components/rack-fader.png";
            s.image_handle = "SurgeXTNative/components/rack-fader-handle.png";
            if (id == 8)
            {
                s.display_mult = 100;
                s.units = "%";
            }
            else
            {
                s.min_value = -8;
                s.max_value = 5;
                s.display_base = 2;
                s.units = " s";
                s.display_precision = 4;
            }
            elements[id] = s;
        }
        else if (id == 10 || id == 12 || id == 52)
        {
            KnobSnapped k;
            k.x_mm = id == 10 ? 9.48 : id == 12 ? 37.48 : 51.48;
            k.y_mm = 55;
            k.default_value = surge_mm::lfo_defaults[id];
            k.image = "SurgeXTNative/components/rack-knob.png";
            if (id == 10)
            {
                k.short_name = "Shape";
                k.num_pos = 8;
                k.pos_names = {"Sine",  "Triangle",      "Square",   "Sawtooth",
                               "Noise", "Sample & Hold", "Envelope", "Step Sequencer"};
            }
            if (id == 12)
            {
                k.short_name = "Deform Type";
                k.num_pos = 3;
                k.pos_names = {"Type 1", "Type 2", "Type 3"};
            }
            if (id == 52)
            {
                k.short_name = "Free / Broadcast Voices";
                k.num_pos = 4;
                k.pos_names = {"1", "2", "3", "4"};
            }
            elements[id] = k;
        }
        else if (id == 11)
        {
            FlipSwitch s;
            s.x_mm = 23.48;
            s.y_mm = 55;
            s.short_name = "Polarity";
            s.num_pos = 2;
            s.frames = {"SurgeXTNative/components/lfo-bi.png",
                        "SurgeXTNative/components/lfo-uni.png"};
            s.pos_names = {"Bipolar", "Unipolar"};
            elements[id] = s;
        }
        else if (id >= 17 && id < 33)
        {
            Slider s;
            names[id] = "Step " + std::to_string(id - 16);
            s.short_name = names[id];
            s.x_mm = 5.8 + 6 * (id - 17);
            s.y_mm = 72;
            s.default_value = surge_mm::lfo_defaults[id];
            s.display_mult = 200;
            s.display_offset = -100;
            s.units = "%";
            s.image = "SurgeXTNative/components/lfo-step.png";
            s.image_handle = "SurgeXTNative/components/lfo-step-handle.png";
            elements[id] = s;
        }
        else if (id >= 33 && id < 49)
        {
            FlipSwitch s;
            names[id] = "Step " + std::to_string(id - 32) + " Triggers";
            s.short_name = names[id];
            s.x_mm = 5.8 + 6 * (id - 33);
            s.y_mm = 84;
            s.num_pos = 4;
            s.frames = {"SurgeXTNative/components/lfo-off.png",
                        "SurgeXTNative/components/lfo-a.png", "SurgeXTNative/components/lfo-b.png",
                        "SurgeXTNative/components/lfo-ab.png"};
            s.pos_names = {"Off", "A", "B", "A and B"};
            elements[id] = s;
        }
        else if (id == 57)
        {
            AltParamContinuous s;
            s.short_name = "Envelope Smoothing";
            s.default_value = .75;
            elements[id] = s;
        }
        else
        {
            AltParamChoiceLabeled a;
            a.num_pos = 2;
            if (id == 13)
            {
                a.short_name = "Clock Targets";
                a.num_pos = 4;
                a.pos_names = {"Off", "LFO Rate", "Envelope Times", "Rate and Envelope"};
                a.default_value = 1;
            }
            if (id == 14)
            {
                a.short_name = "Random Phase on Attack";
                a.pos_names = {"Off", "On"};
            }
            if (id == 15 || id == 16)
            {
                a.short_name = id == 15 ? "Trigger A" : "Trigger B";
                a.num_pos = 5;
                a.pos_names = {"Automatic", "End of Envelope", "Envelope Stage", "Step A",
                               "Step B"};
            }
            if (id == 49 || id == 50)
            {
                a.short_name = id == 49 ? "First Loop Step" : "Last Loop Step";
                a.num_pos = 16;
                a.pos_names = {"1", "2",  "3",  "4",  "5",  "6",  "7",  "8",
                               "9", "10", "11", "12", "13", "14", "15", "16"};
                a.default_value = id == 50 ? 15 : 0;
            }
            if (id == 51)
            {
                a.short_name = "Scale Wave / Envelope by Amplitude";
                a.pos_names = {"Off", "On"};
                a.default_value = 1;
            }
            if (id == 53)
            {
                a.short_name = "Gate Polyphony";
                a.pos_names = {"Follow Gate Cables", "Broadcast Gate Lane 1"};
            }
            if (id == 54)
            {
                a.short_name = "Envelope When Gate Unpatched";
                a.pos_names = {"Mute", "Keep Envelope"};
            }
            if (id == 55)
            {
                a.short_name = "Clock Format";
                a.pos_names = {"Quarter-note pulses", "BPM V/Oct (0 V = 120)"};
            }
            if (id == 56)
            {
                a.short_name = "Retrigger Envelope From";
                a.pos_names = {"Current Level", "Zero"};
            }
            elements[id] = a;
        }
    }
    static const char *inputs[]{
        "Full LFO Gate", "Envelope-only Gate", "Clock / BPM", "Direct Phase (0-10 V)",
        "Rate CV",       "Phase / Shuffle CV", "Deform CV",   "Amplitude CV",
        "Delay CV",      "Attack CV",          "Hold CV",     "Decay CV",
        "Sustain CV",    "Release CV"};
    for (unsigned id = 0; id < 14; ++id)
    {
        JackInput j;
        j.short_name = inputs[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        if (id < 4)
        {
            j.x_mm = 80.5 + 14 * (id % 2);
            j.y_mm = 95 + 13 * (id / 2);
        }
        else
        {
            j.x_mm = 9.48 + 14 * ((id - 4) % 5);
            j.y_mm = 95 + 13 * ((id - 4) / 5);
        }
        elements[58 + id] = j;
        indices[58 + id] = ElementCount::NoElementIndices;
        indices[58 + id].input_idx = id;
    }
    static const char *outputs[]{"Wave x Envelope", "Wave",      "Envelope",
                                 "Cycle Trigger",   "Trigger A", "Trigger B"};
    for (unsigned id = 0; id < 6; ++id)
    {
        JackOutput j;
        j.short_name = outputs[id];
        j.x_mm = 9.3 + 16.6 * id;
        j.y_mm = 121;
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[72 + id] = j;
        indices[72 + id] = ElementCount::NoElementIndices;
        indices[72 + id].output_idx = id;
    }
    DynamicGraphicDisplay d;
    d.short_name = "Live Wave / Envelope";
    d.x_mm = 4;
    d.y_mm = 9;
    d.width_mm = 93;
    d.height_mm = 17;
    d.coords = Coords::TopLeft;
    elements[78] = d;
    indices[78] = ElementCount::NoElementIndices;
    indices[78].light_idx = 0;
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge LFO, DAHDSR envelope and sixteen-step sequencer",
                        .width_hp = 20,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeLFO>("SurgeXTNative", "LFO", info, "SurgeXTNative/LFO.png");
}
