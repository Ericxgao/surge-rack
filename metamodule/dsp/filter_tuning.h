// SPDX-License-Identifier: GPL-3.0-or-later
// 12-TET subset of SurgeStorage.cpp used by src/VCF.h. Preserve its lookup
// interpolation and oversampled omega table even though Rack runs VCF at 1x.
#pragma once
#include <algorithm>
#include <cmath>

namespace surge_mm
{
class FilterTuning
{
    struct PitchTables
    {
        float pitch[512], inverse[512], fraction[1001], inverse_fraction[1001];
        PitchTables()
        {
            for (int i = 0; i < 512; ++i)
            {
                pitch[i] = std::pow(2.f, (float(i) - 256.f) * (1.f / 12.f));
                inverse[i] = 1.f / pitch[i];
            }
            for (int i = 0; i < 1001; ++i)
            {
                double twelfths = i * 1.0 / 12.0 / 1000.0;
                fraction[i] = std::pow(2.0, twelfths);
                inverse_fraction[i] = std::pow(2.0, -twelfths);
            }
        }
    };
    static const PitchTables &tables()
    {
        static const PitchTables t;
        return t;
    }
    float omega[2][512]{};
    static float pitch_lookup(float x, bool inverse)
    {
        const auto &t = tables();
        x = std::clamp(x + 256, inverse ? 0.f : 1.e-4f, 512.f - 1.e-4f);
        int e = int(x);
        float position = (x - float(e)) * 1000.0;
        int index = int(position);
        float fraction = position - index;
        const float *p = inverse ? t.inverse : t.pitch;
        const float *f = inverse ? t.inverse_fraction : t.fraction;
        return p[e] * ((1 - fraction) * f[index] + fraction * f[index + 1]);
    }

  public:
    static constexpr double MIDI_0_FREQ = 8.175798915643707;
    enum TuningMode
    {
        RETUNE_ALL = 0
    };
    TuningMode tuningApplicationMode = RETUNE_ALL;
    struct CurrentTuning
    {
        static double logScaledFrequencyForMidiNote(int n)
        {
            return double(n) / 12.0;
        }
    } currentTuning;
    struct Patch
    {
        bool correctlyTuneCombFilter = true;
    } patch;
    const Patch *_patch = &patch;
    FilterTuning()
    {
        set_samplerate(48000);
    }
    FilterTuning(const FilterTuning &) = delete;
    FilterTuning &operator=(const FilterTuning &) = delete;
    void set_samplerate(float sr)
    {
        const auto &t = tables();
        const double inverse_rate = 1.0 / (double(sr) * 2);
        for (int i = 0; i < 512; ++i)
        {
            double angle =
                2 * 3.14159265358979323846 * std::min(.5, 440 * t.pitch[i] * inverse_rate);
            omega[0][i] = std::sin(angle);
            omega[1][i] = std::cos(angle);
        }
    }
    static float note_to_pitch_ignoring_tuning(float x)
    {
        return pitch_lookup(x, false);
    }
    static float note_to_pitch_inv_ignoring_tuning(float x)
    {
        return pitch_lookup(x, true);
    }
    static float note_to_pitch(float x)
    {
        return note_to_pitch_ignoring_tuning(x);
    }
    void note_to_omega_ignoring_tuning(float x, float &sinu, float &cosi,
                                       float /*sampleRate*/) const
    {
        x = std::clamp(x + 256, 0.f, 512.f - 1.e-4f);
        int e = int(x);
        float a = x - float(e);
        sinu = (1 - a) * omega[0][e] + a * omega[0][(e + 1) & 0x1ff];
        cosi = (1 - a) * omega[1][e] + a * omega[1][(e + 1) & 0x1ff];
    }
};
} // namespace surge_mm
