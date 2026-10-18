// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/FastMath.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <sst/filters/HalfRateFilter.h>
namespace surge_mm::ringmod
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_OS = 16, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
inline float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
float original_sine_shape(float, float, int);
struct RingModStorage : EffectGlobal
{
    double dsamplerate_os_inv{1.0 / 96000};
    alignas(16) float audio_in[2][16]{};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        dsamplerate_os_inv = .5 / sr;
    }
    static float note_to_pitch(float x)
    {
        return FilterTuning::note_to_pitch(x);
    }
};
union RingModValue
{
    float f{};
    int i;
};
struct RingModSettings
{
    struct Parameter
    {
        RingModValue val, val_min, val_max;
        bool deactivated{}, absolute{}, extend_range{};
        float get_extended(float x) const
        {
            return x * (extend_range ? 12.f : 1.f);
        }
    };
    Parameter p[9];
    RingModSettings()
    {
        p[0].val_max.i = 28;
        p[6].val_min.f = -60;
        p[7].val_max.f = 70;
    }
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<RingModStorage, 8>;
struct RingModBase
{
    RingModStorage *storage;
    RingModSettings *fxdata;
    RingModValue *pd;
    float *pd_float[9];
    int *pd_int[9];
    RingModBase(RingModStorage *s, RingModSettings *f, RingModValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 9; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
};
} // namespace surge_mm::ringmod
