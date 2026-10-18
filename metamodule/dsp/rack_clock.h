// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <limits>

namespace surge_mm
{
// ClockProcessor in src/XTModule.h retains the last tempo and Schmitt state
// through disconnects and format changes. Keep those rules for imported patches.
struct RackClock
{
    unsigned gate{2};
    int elapsed{-1};
    float ratio{1}, inverse{1}, last_bpm{-1}, last_volts{-11};
    bool connected{}, synced{};
    bool edge(float volts)
    {
        if (gate == 0 && volts >= 1)
        {
            gate = 1;
            return true;
        }
        if (gate == 1 && volts <= 0)
            gate = 0;
        else if (gate == 2)
        {
            if (volts >= 1)
                gate = 1;
            else if (volts <= 0)
                gate = 0;
        }
        return false;
    }
    void process(float volts, bool patched, bool bpm, float sample_rate)
    {
        if (!patched)
        {
            if (elapsed >= 0 || connected)
                synced = false;
            elapsed = -1;
            connected = false;
            return;
        }
        if (bpm)
        {
            if (!connected)
                synced = true;
            connected = true;
            if (volts != last_volts)
            {
                // Rack computes both fields from the double-precision ratio.
                double value = std::pow(2.0, volts);
                ratio = value;
                inverse = 1.f / value;
            }
            last_volts = volts;
        }
        else
        {
            if (edge(volts))
            {
                if (elapsed > 0 && elapsed < sample_rate * 6)
                {
                    float rate = 60 * sample_rate / elapsed;
                    if (std::abs(rate - std::round(rate)) < .015f)
                        rate = std::round(rate);
                    if (rate != last_bpm)
                    {
                        ratio = rate / 120;
                        inverse = 120 / rate;
                    }
                    last_bpm = rate;
                }
                else
                    synced = true;
                elapsed = 0;
            }
            // Saturate after a very long stopped clock rather than overflowing.
            if (elapsed >= 0 && elapsed < std::numeric_limits<int>::max())
                ++elapsed;
        }
    }
};
} // namespace surge_mm
