// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/Lag.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/dsp/Clippers.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <sst/basic-blocks/mechanics/simd-ops.h>
#include <sst/basic-blocks/tables/SincTableProvider.h>
namespace surge_mm::chorus
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2, max_delay_length = 1 << 18, FIRipol_N = 12,
              FIRipol_M = 256;
template <class T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
struct ChorusStorage : EffectGlobal
{
    float temposyncratio{1}, temposyncratio_inv{1};
    static const sst::basic_blocks::tables::SurgeSincTableProvider &sinc()
    {
        static const sst::basic_blocks::tables::SurgeSincTableProvider table;
        return table;
    }
    const float *sinctable1X = sinc().sinctable1X;
};
union ChorusValue
{
    float f{};
    int i;
};
struct ChorusSettings
{
    struct Parameter
    {
        ChorusValue val;
        bool deactivated{}, temposync{};
    };
    Parameter p[8];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<ChorusStorage, 8>;
struct ChorusBase
{
    ChorusStorage *storage;
    ChorusSettings *fxdata;
    ChorusValue *pd;
    float *pd_float[8];
    ChorusBase(ChorusStorage *s, ChorusSettings *f, ChorusValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 8; ++i)
            pd_float[i] = &v[i].f;
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
// Used only by the host test's independently wrapped delay-reader oracle.
inline __m128 wrapped_read(const float *data, int offset)
{
    return _mm_set_ps(
        data[(offset + 3) & (max_delay_length - 1)], data[(offset + 2) & (max_delay_length - 1)],
        data[(offset + 1) & (max_delay_length - 1)], data[offset & (max_delay_length - 1)]);
}
} // namespace surge_mm::chorus
#include "ChorusHost.h"
