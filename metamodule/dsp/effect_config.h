// SPDX-License-Identifier: GPL-3.0-or-later
// Embedded host for the same SST effect classes used by SurgeSSTFXAdapter.h.
#pragma once
#include "filter_tuning.h"
#include "modulation_clock.h"
#include <cstdlib>
#include <sst/filters/BiquadFilter.h>
namespace surge_mm
{
struct EffectGlobal : EnvelopeClock
{
    double dsamplerate_inv{1.0 / 48000};
    float tempo_ratio{1};
    struct DecibelTable
    {
        float values[512];
        DecibelTable()
        {
            for (int i = 0; i < 512; ++i)
                values[i] = std::pow(10.f, .05f * (float(i) - 384));
        }
    };
    EffectGlobal()
    {
        (void)db_table();
    }
    static const DecibelTable &db_table()
    {
        static const DecibelTable table;
        return table;
    }
    void set_samplerate(float sr)
    {
        EnvelopeClock::set_samplerate(sr);
        dsamplerate_inv = 1.0 / sr;
    }
    static float note_to_pitch_ignoring_tuning(float x)
    {
        return FilterTuning::note_to_pitch_ignoring_tuning(x);
    }
    static float db_to_linear(float x)
    {
        x += 384;
        int e = int(x);
        float a = x - e;
        const auto &v = db_table().values;
        return (1 - a) * v[e & 511] + a * v[(e + 1) & 511];
    }
    float envelope_rate_linear(float x) const
    {
        x = x * 16 + 256;
        int e = int(x);
        float a = x - e;
        return (1 - a) * rates[e & 511] + a * rates[(e + 1) & 511];
    }
};
struct EffectSettings
{
    std::array<bool, 12> deactivated{}, extended{}, synced{};
};
struct EffectConfig
{
    static constexpr uint16_t blockSize = 8;
    using GlobalStorage = EffectGlobal;
    using EffectStorage = EffectSettings;
    using ValueStorage = float;
    struct BaseClass
    {
        BaseClass(GlobalStorage *, EffectStorage *, ValueStorage *)
        {
        }
    };
    using BiquadAdapter = sst::filters::Biquad::DefaultTuningAndDBAdapter<GlobalStorage>;
    static float floatValueAt(const BaseClass *const, const ValueStorage *const v, int i)
    {
        return v[i];
    }
    static int intValueAt(const BaseClass *const, const ValueStorage *const v, int i)
    {
        return int(std::round(v[i]));
    }
    static float envelopeRateLinear(GlobalStorage *s, float f)
    {
        return s->envelope_rate_linear(f);
    }
    static float temposyncRatio(GlobalStorage *s, EffectStorage *e, int i)
    {
        return e->synced[i] ? s->tempo_ratio : 1;
    }
    static bool isDeactivated(EffectStorage *e, int i)
    {
        return e->deactivated[i];
    }
    static bool isExtended(EffectStorage *e, int i)
    {
        return e->extended[i];
    }
    static float rand01(GlobalStorage *)
    {
        return float(std::rand()) / RAND_MAX;
    }
    static double sampleRate(GlobalStorage *s)
    {
        return s->samplerate;
    }
    static float noteToPitch(GlobalStorage *, float p)
    {
        return FilterTuning::note_to_pitch(p);
    }
    static float noteToPitchIgnoringTuning(GlobalStorage *, float p)
    {
        return FilterTuning::note_to_pitch_ignoring_tuning(p);
    }
    static float noteToPitchInv(GlobalStorage *, float p)
    {
        return FilterTuning::note_to_pitch_inv_ignoring_tuning(p);
    }
    static float dbToLinear(GlobalStorage *s, float f)
    {
        return s->db_to_linear(f);
    }
};
} // namespace surge_mm
