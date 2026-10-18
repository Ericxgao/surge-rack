// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "utility_dsp.h"
namespace surge_mm
{
struct TunedDelay;
TunedDelay *create_tuned_delay();
void destroy_tuned_delay(TunedDelay *);
void reset_tuned_delay(TunedDelay *);
void tuned_delay_samplerate(TunedDelay *, float);
void process_tuned_delay(TunedDelay *, const StereoPoly &, const PolyValues &pitch_notes,
                         const PolyValues &correction_samples, unsigned channels, StereoPoly &);
} // namespace surge_mm
