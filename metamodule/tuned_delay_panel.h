// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tuned_delay_processor.h"
#include "panel_controls.h"
#include "CoreModules/register_module.hh"
inline void register_tuned_delay()
{
    using namespace MetaModule;
    static std::array<Element, 8> elements;
    static std::array<ElementCount::Indices, 8> indices;
    Knob pitch;
    pitch.x_mm = 15.24f;
    pitch.y_mm = 28;
    pitch.short_name = "V/Oct Center";
    pitch.default_value = .5;
    pitch.display_mult = 10;
    pitch.display_offset = -5;
    pitch.units = "oct";
    pitch.image = "SurgeXTNative/components/rack-big-knob.png";
    elements[0] = pitch;
    KnobSnapped correction;
    correction.x_mm = 15.24f;
    correction.y_mm = 52;
    correction.short_name = "Sample Correction";
    correction.default_value = 0;
    correction.num_pos = 21;
    correction.image = "SurgeXTNative/components/rack-big-knob.png";
    static std::array<std::string, 21> labels;
    for (unsigned n = 0; n < 21; ++n)
    {
        labels[n] = std::to_string(n);
        correction.pos_names[n] = labels[n].c_str();
    }
    elements[1] = correction;
    for (unsigned n = 0; n < 2; ++n)
    {
        indices[n] = ElementCount::NoElementIndices;
        indices[n].param_idx = n;
    }
    for (unsigned n = 0; n < 4; ++n)
    {
        JackInput j;
        j.x_mm = 8.24f + 14 * (n % 2);
        j.y_mm = n < 2 ? 101 : 78;
        static const char *names[]{"Audio Left", "Audio Right", "Pitch V/Oct",
                                   "Sample Correction CV"};
        j.short_name = names[n];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[2 + n] = j;
        indices[2 + n] = ElementCount::NoElementIndices;
        indices[2 + n].input_idx = n;
    }
    for (unsigned n = 0; n < 2; ++n)
    {
        JackOutput j;
        j.x_mm = 8.24f + 14 * n;
        j.y_mm = 116;
        j.short_name = n ? "Right" : "Left";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[6 + n] = j;
        indices[6 + n] = ElementCount::NoElementIndices;
        indices[6 + n].output_idx = n;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge sinc-interpolated delay tuned by V/Oct",
                        .width_hp = 6,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeTunedDelay>("SurgeXTNative", "DelayLineByFreq", info,
                                     "SurgeXTNative/DelayLineByFreq.png");
}
