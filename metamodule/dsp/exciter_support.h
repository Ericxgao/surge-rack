// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <chowdsp/shared/Oversampling.h>
#include <chowdsp/exciter/LevelDetector.h>
namespace surge_mm::exciter
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
inline float clamp01(float value)
{
    return std::clamp(value, 0.f, 1.f);
}
struct ExciterStorage : EffectGlobal
{
    float samplerate_inv{1.f / 48000};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        samplerate_inv = 1.f / sr;
    }
};
union ExciterValue
{
    float f{};
    int i;
};
struct ExciterSettings
{
    struct Parameter
    {
        struct Display
        {
            float b{std::log2(100.f)};
        } displayInfo;
    };
    Parameter p[5];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<ExciterStorage, 8>;
struct ExciterBase
{
    ExciterStorage *storage;
    ExciterSettings *fxdata;
    ExciterValue *pd;
    float *pd_float[5];
    ExciterBase(ExciterStorage *s, ExciterSettings *f, ExciterValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 5; ++i)
            pd_float[i] = &v[i].f;
    }
};
} // namespace surge_mm::exciter
