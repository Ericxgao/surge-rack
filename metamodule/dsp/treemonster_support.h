// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/QuadratureOscillators.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
namespace surge_mm::treemonster
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
constexpr float BLOCK_SIZE_INV = 1.f / 8;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
inline float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
inline float clamp1bp(float x)
{
    return std::clamp(x, -1.f, 1.f);
}
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
struct TreeMonsterStorage : EffectGlobal
{
    double dsamplerate{48000};
    float samplerate_inv{1.f / 48000};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        dsamplerate = sr;
        samplerate_inv = 1.f / sr;
    }
};
union TreeMonsterValue
{
    float f{};
    int i;
};
struct TreeMonsterSettings
{
    struct Parameter
    {
        TreeMonsterValue val, val_min, val_max;
        bool deactivated{};
    };
    Parameter p[8];
    TreeMonsterSettings()
    {
        p[0].val_min.f = -96;
        p[0].val_max.f = 0;
        p[2].val_min.f = -60;
        p[3].val_max.f = 70;
    }
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<TreeMonsterStorage, 8>;
struct TreeMonsterBase
{
    TreeMonsterStorage *storage;
    TreeMonsterSettings *fxdata;
    TreeMonsterValue *pd;
    float *pd_float[8];
    TreeMonsterBase(TreeMonsterStorage *s, TreeMonsterSettings *f, TreeMonsterValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 8; ++i)
            pd_float[i] = &v[i].f;
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
} // namespace surge_mm::treemonster
#include "TreeMonsterHost.h"
