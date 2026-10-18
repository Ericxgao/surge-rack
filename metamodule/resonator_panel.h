// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "resonator_processor.h"
#include "CoreModules/register_module.hh"
inline void register_resonator()
{
    using namespace MetaModule;
    static std::array<Element, 35> elements;
    static std::array<ElementCount::Indices, 35> indices;
    static std::array<std::string, 11> names;
    for (unsigned slot = 0; slot < 11; ++slot)
    {
        unsigned id = surge_mm::resonator_cv_ids[slot];
        Knob k;
        k.x_mm = id < 9 ? 9.48f + 14 * (id % 3) : 51.48f;
        k.y_mm = id < 9 ? 25 + 17 * (id / 3) : id == 10 ? 42 : 59;
        k.short_name = surge_mm::resonator_names[id];
        k.default_value = surge_mm::resonator_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = "SurgeXTNative/components/rack-knob.png";
        if (id == 10)
        {
            k.display_mult = 96;
            k.display_offset = -48;
            k.units = "dB";
        }
        elements[slot] = k;
        indices[slot] = ElementCount::NoElementIndices;
        indices[slot].param_idx = id;
    }
    AltParamChoiceLabeled mode;
    mode.short_name = "Mode";
    mode.num_pos = 4;
    mode.default_value = 1;
    mode.pos_names = {"Lowpass", "Bandpass", "Bandpass + - +", "Highpass"};
    elements[11] = mode;
    indices[11] = ElementCount::NoElementIndices;
    indices[11].param_idx = 9;
    static const char *options[]{"Band 1 Frequency Range",
                                 "Band 2 Frequency Range",
                                 "Band 3 Frequency Range",
                                 "Band 1 Resonance CV",
                                 "Band 2 Resonance CV",
                                 "Band 3 Resonance CV",
                                 "Polyphony"};
    for (unsigned i = 0; i < 7; ++i)
    {
        AltParamChoiceLabeled a;
        a.short_name = options[i];
        a.num_pos = 2;
        if (i < 3)
            a.pos_names = {"Normal", "Full 60-7500 Hz"};
        else if (i < 6)
            a.pos_names = {"Limit to 0-100%", "Extended"};
        else
            a.pos_names = {"Sum to stereo", "Polyphonic"};
        elements[12 + i] = a;
        indices[12 + i] = ElementCount::NoElementIndices;
        indices[12 + i].param_idx = 12 + i;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[19 + id] = j;
        indices[19 + id] = ElementCount::NoElementIndices;
        indices[19 + id].input_idx = id;
    }
    // Match each band row across frequency, resonance and gain, with global CVs at right.
    constexpr std::array<unsigned, 11> order{0, 1, 2, 9, 3, 4, 5, 10, 6, 7, 8};
    for (unsigned slot = 0; slot < 11; ++slot)
    {
        unsigned cv = order[slot], id = surge_mm::resonator_cv_ids[cv];
        bool freq = id < 9 && id % 3 == 0;
        names[cv] = std::string(surge_mm::resonator_names[id]) + (freq ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 77 + 14 * (slot / 4);
        j.short_name = names[cv];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[21 + cv] = j;
        indices[21 + cv] = ElementCount::NoElementIndices;
        indices[21 + cv].input_idx = 2 + cv;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[32 + id] = j;
        indices[32 + id] = ElementCount::NoElementIndices;
        indices[32 + id].output_idx = id;
    }
    DynamicTextDisplay tuning;
    tuning.x_mm = 4;
    tuning.y_mm = 10;
    tuning.width_mm = 54;
    tuning.height_mm = 7;
    tuning.coords = Coords::TopLeft;
    tuning.short_name = "Band Frequencies";
    tuning.font = "Default_10";
    tuning.color = Colors565::Orange;
    elements[34] = tuning;
    indices[34] = ElementCount::NoElementIndices;
    indices[34].light_idx = 0;
    ModuleInfoView info{.description = "Surge three-band Resonator with eleven direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeResonator>("SurgeXTNative", "Resonator", info,
                                    "SurgeXTNative/Resonator.png");
}
