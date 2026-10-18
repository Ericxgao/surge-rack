// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "panel_controls.h"
#include "modulation_processor.h"
#include "CoreModules/register_module.hh"
inline void register_quad_lfo()
{
    using namespace MetaModule;
    static std::array<Element, 35> elements;
    static std::array<ElementCount::Indices, 35> indices;
    static std::array<std::string, 35> names;
    SurgeQuadLFO defaults;
    for (unsigned id = 0; id < 19; ++id)
    {
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
        unsigned stage = id % 4;
        float x = 9.48f + stage * 14;
        if (id < 8)
        {
            Knob knob;
            names[id] = std::string(id < 4 ? "Rate/Relation " : "Deform/Spread ") +
                        std::to_string(stage + 1);
            knob.short_name = names[id];
            knob.x_mm = x;
            knob.y_mm = id < 4 ? 39 : 55;
            knob.image = "SurgeXTNative/components/rack-knob.png";
            knob.default_value = defaults.get_param(id);
            knob.display_mult = id < 4 ? 100 : 200;
            knob.display_offset = id < 4 ? 0 : -100;
            knob.units = "%";
            elements[id] = knob;
        }
        else if (id < 12)
        {
            KnobSnapped shape;
            names[id] = "Shape " + std::to_string(stage + 1);
            shape.short_name = names[id];
            shape.x_mm = x;
            shape.y_mm = 25;
            shape.num_pos = 8;
            shape.default_value = 0;
            shape.pos_names = {"Sine",  "Ramp Up", "Ramp Down", "Triangle",
                               "Pulse", "Random",  "S&H",       "Random Trigger"};
            shape.image = "SurgeXTNative/components/rack-small-knob.png";
            elements[id] = shape;
        }
        else if (id < 16)
        {
            FlipSwitch polarity;
            names[id] = "Polarity " + std::to_string(stage + 1);
            polarity.short_name = names[id];
            polarity.x_mm = x + 6;
            polarity.y_mm = 25;
            polarity.num_pos = 2;
            polarity.default_value = 1;
            polarity.pos_names = {"Unipolar", "Bipolar"};
            polarity.frames = {"SurgeXTNative/components/lfo-uni.png",
                               "SurgeXTNative/components/lfo-bi.png"};
            elements[id] = polarity;
        }
        else if (id == 16)
        {
            KnobSnapped mode;
            mode.short_name = "Interaction Mode";
            mode.x_mm = 30.48f;
            mode.y_mm = 12;
            mode.num_pos = 5;
            mode.pos_names = {"Independent", "Rate Ratio", "Quadrature", "Phase Offset",
                              "Entangled"};
            mode.default_value = 0;
            mode.image = "SurgeXTNative/components/rack-small-knob.png";
            elements[id] = mode;
        }
        else
        {
            AltParamChoiceLabeled option;
            if (id == 17)
            {
                option.short_name = "Polyphony";
                option.num_pos = 5;
                option.pos_names = {"Follow Reset", "1", "2", "3", "4"};
            }
            else
            {
                option.short_name = "Clock Format";
                option.num_pos = 2;
                option.pos_names = {"Quarter-note pulses", "BPM V/Oct"};
            }
            elements[id] = option;
        }
    }
    for (unsigned id = 0; id < 12; ++id)
    {
        JackInput jack;
        unsigned stage = id % 4;
        static const char *triggers[]{"Reset 1 / All", "Reset 2 / Clock", "Reset 3 / Freeze",
                                      "Reset 4 / Reverse"};
        names[19 + id] = id < 4 ? triggers[id]
                                : std::string(id < 8 ? "Rate/Relation CV " : "Deform/Spread CV ") +
                                      std::to_string(stage + 1);
        jack.short_name = names[19 + id];
        jack.x_mm = 9.48f + stage * 14;
        jack.y_mm = id < 4 ? 71 : id < 8 ? 85 : 100;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[19 + id] = jack;
        indices[19 + id] = ElementCount::NoElementIndices;
        indices[19 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 4; ++id)
    {
        JackOutput jack;
        names[31 + id] = "LFO " + std::to_string(id + 1);
        jack.short_name = names[31 + id];
        jack.x_mm = 9.48f + id * 14;
        jack.y_mm = 114.5f;
        jack.image = "SurgeXTNative/components/rack-jack.png";
        elements[31 + id] = jack;
        indices[31 + id] = ElementCount::NoElementIndices;
        indices[31 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Four Surge LFOs with clocked interaction modes",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeQuadLFO>("SurgeXTNative", "QuadLFO", info, "SurgeXTNative/QuadLFO.png");
}
