// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include "VocoderIntrinsics.h"
namespace surge_mm::vocoder
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
struct VocoderStorage : EffectGlobal
{
    float samplerate_inv{1.f / 48000};
    alignas(16) float audio_in_nonOS[2][8]{};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        samplerate_inv = 1.f / sr;
    }
};
union VocoderValue
{
    float f{};
    int i;
};
struct VocoderSettings
{
    struct Parameter
    {
        VocoderValue val;
    };
    Parameter p[12];
};
struct VocoderBase
{
    VocoderStorage *storage;
    VocoderSettings *fxdata;
    VocoderValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    VocoderBase(VocoderStorage *s, VocoderSettings *f, VocoderValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
};
} // namespace surge_mm::vocoder
