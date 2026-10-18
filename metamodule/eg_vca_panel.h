// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "eg_vca_processor.h"
#include "CoreModules/register_module.hh"
inline void register_eg_vca()
{
    using namespace MetaModule;
    static std::array<Element, 33> elements;
    static std::array<ElementCount::Indices, 33> indices;
    static const char *names[]{"Level",        "Pan",          "Response",       "Attack/Delay",
                               "Decay/Attack", "Sustain/Hold", "Release",        "Digital/Analog",
                               "Attack Curve", "Decay Curve",  "Release Curve",  "ADSR/DAHD",
                               "Attack From",  "Time Range",   "Stereo Pan Law", "Event Trigger",
                               "Clock Format"};
    SurgeEGxVCA defaults;
    for (unsigned id = 0; id < 17; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        if (id < 3)
        {
            Knob knob;
            knob.short_name = names[id];
            knob.default_value = defaults.get_param(id);
            knob.x_mm = id == 0 ? 51.48f : 37.48f;
            knob.y_mm = id == 0 ? 47 : id == 1 ? 39 : 55;
            knob.image = "SurgeXTNative/components/rack-knob.png";
            knob.display_mult = id < 2 ? 200 : 100;
            knob.display_offset = id == 1 ? -100 : 0;
            knob.units = "%";
            elements[id] = knob;
        }
        else if (id < 7)
        {
            Slider slider;
            slider.short_name = names[id];
            slider.default_value = defaults.get_param(id);
            slider.x_mm = 5.98f + (id - 3) * 7;
            slider.y_mm = 47;
            slider.display_mult = 100;
            slider.units = "%";
            slider.image = "SurgeXTNative/components/rack-fader.png";
            slider.image_handle = "SurgeXTNative/components/rack-fader-handle.png";
            elements[id] = slider;
        }
        else if (id < 12 || id == 13)
        {
            FlipSwitch button;
            button.short_name = names[id];
            button.num_pos = 2;
            button.x_mm = id == 7 ? 9.48f : id == 11 ? 51.48f : 30.48f;
            button.y_mm = 12;
            if (id == 7)
            {
                button.pos_names = {"Digital", "Analog"};
                button.frames = {"SurgeXTNative/components/eg-digital.png",
                                 "SurgeXTNative/components/eg-analog.png"};
            }
            else if (id == 11)
            {
                button.pos_names = {"ADSR", "DAHD"};
                button.frames = {"SurgeXTNative/components/eg-adsr.png",
                                 "SurgeXTNative/components/eg-dahd.png"};
            }
            else if (id == 13)
            {
                button.pos_names = {"Fast (10s)", "Slow (120s)"};
                button.frames = {"SurgeXTNative/components/eg-fast.png",
                                 "SurgeXTNative/components/eg-slow.png"};
            }
            else
            {
                button.x_mm = 5.98f + (id - 8) * 7;
                button.y_mm = 27;
                button.num_pos = 3;
                button.default_value = 1;
                button.pos_names = {"Faster", "Standard", "Slower"};
                button.frames = {"SurgeXTNative/components/ad-fast.png",
                                 "SurgeXTNative/components/ad-linear.png",
                                 "SurgeXTNative/components/ad-slow.png"};
            }
            elements[id] = button;
        }
        else
        {
            AltParamChoiceLabeled option;
            option.short_name = names[id];
            option.num_pos = 2;
            if (id == 12)
            {
                option.pos_names = {"Zero", "Current Value"};
                option.default_value = 1;
            }
            if (id == 14)
                option.pos_names = {"Stereo Balance", "Stereo Pan"};
            if (id == 15)
            {
                option.num_pos = 8;
                option.pos_names = {"End of Cycle",           "Start of Cycle",
                                    "Start of Attack (DAHD)", "Start of Hold (DAHD)",
                                    "Start of Decay (ADSR)",  "Start of Sustain (ADSR)",
                                    "Start of Release",       "All Transitions"};
            }
            if (id == 16)
                option.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
            elements[id] = option;
        }
    }
    static const char *input_names[]{
        "Left",      "Right",       "Gate/Trigger",    "Clock/BPM",       "Level CV",
        "Pan CV",    "Response CV", "Attack/Delay CV", "Decay/Attack CV", "Sustain/Hold CV",
        "Release CV"};
    for (unsigned id = 0; id < 11; ++id)
    {
        JackInput jack;
        jack.short_name = input_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        if (id < 2)
        {
            jack.x_mm = 9.48f + id * 14;
            jack.y_mm = 114.5f;
        }
        else if (id < 4)
        {
            jack.x_mm = 9.48f + (id - 2) * 14;
            jack.y_mm = 71;
        }
        else
        {
            jack.x_mm = 9.48f + (id < 7 ? id - 4 : id - 7) * 14;
            jack.y_mm = id < 7 ? 85 : 100;
        }
        elements[17 + id] = jack;
        indices[17 + id] = ElementCount::NoElementIndices;
        indices[17 + id].input_idx = id;
    }
    static const char *output_names[]{"Left", "Right", "Envelope", "Event"};
    for (unsigned id = 0; id < 4; ++id)
    {
        JackOutput jack;
        jack.short_name = output_names[id];
        jack.image = "SurgeXTNative/components/rack-jack.png";
        jack.x_mm = id < 2 ? 37.48f + id * 14 : id == 2 ? 51.48f : 37.48f;
        jack.y_mm = id < 2 ? 114.5f : 71;
        elements[28 + id] = jack;
        indices[28 + id] = ElementCount::NoElementIndices;
        indices[28 + id].output_idx = id;
    }
    DynamicGraphicDisplay trace;
    trace.x_mm = 4.5f;
    trace.y_mm = 17;
    trace.width_mm = 52;
    trace.height_mm = 7;
    trace.coords = Coords::TopLeft;
    trace.short_name = "Envelope trace";
    elements[32] = trace;
    indices[32] = ElementCount::NoElementIndices;
    indices[32].light_idx = 0;
    static constexpr std::array<ModuleInfoBase::BypassRoute, 2> bypass{{{0, 0}, {1, 1}}};
    ModuleInfoView info{.description = "Surge ADSR/DAHD envelope and stereo VCA",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices,
                        .bypass_routes = bypass};
    register_module<SurgeEGxVCA>("SurgeXTNative", "EGxVCA", info, "SurgeXTNative/EGxVCA.png");
}
