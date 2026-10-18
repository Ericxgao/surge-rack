// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/Lag.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/dsp/QuadratureOscillators.h>
#include <sst/basic-blocks/tables/SincTableProvider.h>
#include <sst/waveshapers.h>
namespace surge_mm::rotary
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2, max_delay_length = 1 << 18, FIRipol_N = 12,
              FIRipol_M = 256;
template <class T, bool first = true> using lipol = sst::basic_blocks::dsp::lipol<T, 8, first>;
template <class T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
struct RotaryStorage : EffectGlobal
{
    float temposyncratio{1};
    static const sst::basic_blocks::tables::SurgeSincTableProvider &sinc()
    {
        static const sst::basic_blocks::tables::SurgeSincTableProvider table;
        return table;
    }
    const float *sinctable1X = sinc().sinctable1X;
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
};
union RotaryValue
{
    float f{};
    int i;
};
struct RotarySettings
{
    struct Parameter
    {
        RotaryValue val;
        bool deactivated{}, temposync{};
    };
    Parameter p[8];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<RotaryStorage, 8>;
struct RotaryBase
{
    RotaryStorage *storage;
    RotarySettings *fxdata;
    RotaryValue *pd;
    float *pd_float[8];
    int *pd_int[8];
    RotaryBase(RotaryStorage *s, RotarySettings *f, RotaryValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 8; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
} // namespace surge_mm::rotary
#include "RotaryHost.h"
