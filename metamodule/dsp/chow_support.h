// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <chowdsp/shared/SmoothedValue.h>
#include <chowdsp/shared/Oversampling.h>
namespace surge_mm::chow
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
using ChowStorage = EffectGlobal;
union ChowValue
{
    float f{};
    int i;
    bool b;
};
struct ChowSettings
{
    struct Parameter
    {
        ChowValue val, val_min, val_max;
    };
    Parameter p[4];
};
struct ChowBase
{
    ChowStorage *storage;
    ChowSettings *fxdata;
    ChowValue *pd;
    float *pd_float[4];
    ChowBase(ChowStorage *s, ChowSettings *f, ChowValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 4; ++i)
            pd_float[i] = &v[i].f;
    }
};
} // namespace surge_mm::chow
