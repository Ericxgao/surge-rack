// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "modulation_clock.h"
#include "rack_clock.h"

namespace surge_mm
{
// Rack clocks belong to the module, independently of its audio voice engines.
// Preserve their history when voice count or sample rate resets those engines.
struct FXClock
{
    TempoClock native;
    RackClock imported;
    bool rack_mode{}, synced{};
    float ratio{1}, inverse{1};
    void set_rack_mode(bool enabled)
    {
        native = {};
        imported = {};
        rack_mode = enabled;
        synced = false;
        ratio = inverse = 1;
        sample_rate_changed();
    }
    void sample_rate_changed()
    {
        if (rack_mode)
        {
            // XTModule::onSampleRateChange forces its legacy clock CV (-100V)
            // through the 1..1024 BPM clamp, before ClockProcessor next runs.
            ratio = imported.ratio = 1.f / 120;
            inverse = imported.inverse = 1 / ratio;
        }
    }
    void reset_engine()
    {
        if (!rack_mode)
            set_rack_mode(false);
    }
    void process(float volts, bool connected, bool bpm, float sample_rate)
    {
        if (rack_mode)
        {
            imported.process(volts, connected, bpm, sample_rate);
            synced = imported.synced;
            ratio = imported.ratio;
            inverse = imported.inverse;
        }
        else
        {
            native.process(volts, connected, bpm, sample_rate);
            synced = native.synced;
            ratio = native.ratio;
            inverse = 1 / native.ratio;
        }
    }
};
} // namespace surge_mm
