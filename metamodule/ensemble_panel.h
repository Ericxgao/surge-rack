// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ensemble_processor.h"
#include "CoreModules/register_module.hh"
#include "panel_controls.h"
inline void register_ensemble()
{
    using namespace MetaModule;
    static std::array<Element, 30> elements;
    static std::array<ElementCount::Indices, 30> indices;
    static std::array<std::string, 12> names;
    constexpr std::array<std::array<float, 2>, 12> xy{{{9.48f, 43},
                                                       {16.48f, 27},
                                                       {37.48f, 43},
                                                       {44.48f, 27},
                                                       {51.48f, 43},
                                                       {9.48f, 11},
                                                       {23.48f, 43},
                                                       {23.48f, 59},
                                                       {9.48f, 59},
                                                       {37.48f, 59},
                                                       {51.48f, 59},
                                                       {51.48f, 11}}};
    for (unsigned id = 0; id < 12; ++id)
    {
        Knob k;
        k.x_mm = xy[id][0];
        k.y_mm = xy[id][1];
        k.short_name = surge_mm::ensemble_names[id];
        k.default_value = surge_mm::ensemble_defaults[id];
        k.display_mult = 100;
        k.units = "%";
        k.image = id == 1 || id == 3 ? "SurgeXTNative/components/rack-knob-12.png"
                  : id == 11         ? "SurgeXTNative/components/rack-small-knob.png"
                                     : "SurgeXTNative/components/rack-knob.png";
        if (id == 0 || id == 11)
        {
            k.display_base = std::exp2(130.f / 12);
            k.display_mult = 13.75;
            k.display_precision = 5;
            k.units = "Hz";
        }
        if (id == 1 || id == 3)
        {
            k.display_base = 2000;
            k.display_mult = .01;
            k.display_precision = 3;
            k.units = "Hz";
        }
        if (id == 6)
        {
            k.display_mult = 98.5;
            k.display_offset = 1.5;
            k.display_precision = 4;
            k.units = "kHz";
        }
        if (id == 9)
        {
            k.display_mult = 200;
            k.display_offset = -100;
        }
        elements[id] = k;
        if (id == 5)
        {
            KnobSnapped s;
            s.x_mm = k.x_mm;
            s.y_mm = k.y_mm;
            s.short_name = "Delay Type";
            s.default_value = surge_mm::ensemble_defaults[5];
            s.num_pos = 7;
            s.image = "SurgeXTNative/components/rack-small-knob.png";
            s.pos_names = {"Digital Delay", "BBD 128",  "BBD 256", "BBD 512",
                           "BBD 1024",      "BBD 2048", "BBD 4096"};
            elements[id] = s;
        }
        indices[id] = ElementCount::NoElementIndices;
        indices[id].param_idx = id;
    }
    FlipSwitch enabled;
    enabled.x_mm = 41;
    enabled.y_mm = 11;
    enabled.short_name = "Output Filter Enabled";
    enabled.default_value = 1;
    enabled.num_pos = 2;
    enabled.pos_names = {"Off", "On"};
    enabled.frames = {"SurgeXTNative/components/filter-off.png",
                      "SurgeXTNative/components/filter-on.png"};
    elements[12] = enabled;
    indices[12] = ElementCount::NoElementIndices;
    indices[12].param_idx = 12;
    AltParamChoiceLabeled poly;
    poly.short_name = "Polyphony";
    poly.num_pos = 2;
    poly.pos_names = {"Sum to stereo", "Polyphonic"};
    elements[13] = poly;
    indices[13] = ElementCount::NoElementIndices;
    indices[13].param_idx = 13;
    constexpr std::array<unsigned, 12> order{1, 3, 2, 4, 0, 6, 8, 7, 9, 10, 5, 11};
    for (unsigned slot = 0; slot < 12; ++slot)
    {
        auto id = order[slot];
        names[id] =
            std::string(surge_mm::ensemble_names[id]) + (id == 0 || id == 11 ? " V/Oct" : " CV");
        JackInput j;
        j.x_mm = 9.48f + 14 * (slot % 4);
        j.y_mm = 76 + 14 * (slot / 4);
        j.short_name = names[id];
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[16 + id] = j;
        indices[16 + id] = ElementCount::NoElementIndices;
        indices[16 + id].input_idx = 2 + id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackInput j;
        j.x_mm = 9.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Audio Left" : "Audio Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[14 + id] = j;
        indices[14 + id] = ElementCount::NoElementIndices;
        indices[14 + id].input_idx = id;
    }
    for (unsigned id = 0; id < 2; ++id)
    {
        JackOutput j;
        j.x_mm = 37.48f + 14 * id;
        j.y_mm = 117;
        j.short_name = id == 0 ? "Left / Mono" : "Right";
        j.image = "SurgeXTNative/components/rack-jack.png";
        elements[28 + id] = j;
        indices[28 + id] = ElementCount::NoElementIndices;
        indices[28 + id].output_idx = id;
    }
    surge_mm::configure_snapped_knob_ranges(elements);
    ModuleInfoView info{.description = "Surge BBD Ensemble with twelve direct CV inputs",
                        .width_hp = 12,
                        .elements = elements,
                        .indices = indices};
    register_module<SurgeEnsemble>("SurgeXTNative", "Ensemble", info, "SurgeXTNative/Ensemble.png");
}
