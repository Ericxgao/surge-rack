// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace surge_mm
{
// The interpolated envelope-rate table used by SurgeStorage, block size eight.
struct EnvelopeClock
{
    float samplerate{48000};
    std::array<float, 512> rates{};
    EnvelopeClock()
    {
        set_samplerate(samplerate);
    }
    void set_samplerate(float sr)
    {
        samplerate = sr;
        for (unsigned i = 0; i < 512; ++i)
            rates[i] = float(8.0 / (double(sr) * std::pow(2.0, (int(i) - 256.0) / 16.0)));
    }
    float envelope_rate_linear_nowrap(float x) const
    {
        x = x * 16 + 256;
        int index = std::clamp(int(x), 0, 510);
        float fraction = x - index;
        return (1 - fraction) * rates[index] + fraction * rates[index + 1];
    }
};
struct Edge
{
    bool high{};
    bool process(float value)
    {
        if (high)
        {
            if (value <= 0)
                high = false;
            return false;
        }
        if (value >= 1)
        {
            high = true;
            return true;
        }
        return false;
    }
};
// Rack clock formats, kept outside the envelope engines.
struct TempoClock
{
    Edge edge;
    int elapsed{-1}, last_style{-1};
    float ratio{1};
    bool synced{};
    void reset()
    {
        edge = {};
        elapsed = -1;
        synced = false;
    }
    void process(float volts, bool connected, bool bpm_volts, float sr)
    {
        if (!connected || int(bpm_volts) != last_style)
            reset();
        last_style = bpm_volts;
        if (!connected)
            return;
        if (bpm_volts)
        {
            synced = true;
            ratio = std::exp2(std::clamp(volts, -10.f, 10.f));
            return;
        }
        if (edge.process(volts))
        {
            if (elapsed > 0 && elapsed < sr * 6)
            {
                float bpm = 60 * sr / elapsed;
                if (std::abs(bpm - std::round(bpm)) < .015f)
                    bpm = std::round(bpm);
                ratio = bpm / 120;
            }
            synced = true;
            elapsed = 0;
        }
        if (elapsed >= 0 && elapsed < sr * 6 + 1)
            ++elapsed;
    }
};
inline float round_tempo_time(float value)
{
    float whole = std::floor(value), fraction = std::exp2(value - whole);
    return whole + (fraction > 1.41f    ? std::log2(1.5f)
                    : fraction > 1.167f ? std::log2(4.f / 3)
                                        : 0);
}
} // namespace surge_mm
