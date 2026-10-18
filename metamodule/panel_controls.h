// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "CoreModules/elements/elements.hh"
#include <span>

namespace surge_mm
{
// SDK 2.4 manual control uses these integer endpoints for its encoder arc.
// num_pos alone controls snapping/labels, but leaves the arc at 0..1.
// The firmware still sends normalized 0..1 values to set_param(), just as
// mapped knobs do. Keep processor values and saved patches normalized.
inline void configure_snapped_knob_ranges(std::span<MetaModule::Element> elements)
{
    for (auto &element : elements)
        if (auto *knob = std::get_if<MetaModule::KnobSnapped>(&element); knob && knob->num_pos > 1)
        {
            knob->min_value = 0;
            knob->max_value = float(knob->num_pos - 1);
        }
}
} // namespace surge_mm
