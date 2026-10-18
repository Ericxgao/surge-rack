// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <memory>
#include <new>
#include <utility>
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/dsp/SSESincDelayLine.h>
#include <sst/basic-blocks/modulators/FXModControl.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <sst/basic-blocks/tables/SincTableProvider.h>
#include <sst/waveshapers.h>
// Leaf objects keep their address; callers reconnect their WDF tree immediately.
// Their constructors cannot throw after allocation (all names are string literals).
template <class T, class... Args> void ensemble_reuse(std::unique_ptr<T> &p, Args &&...args)
{
    if (!p)
        p = std::make_unique<T>(std::forward<Args>(args)...);
    else
    {
        p->~T();
        ::new (static_cast<void *>(p.get())) T(std::forward<Args>(args)...);
    }
}
#include <chowdsp/bbd_utils/BBDDelayLine.h>
#include <chowdsp/bbd_utils/BBDNonlin.h>
namespace surge_mm::ensemble
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
using ModControl = sst::basic_blocks::modulators::FXModControl<8>;
template <size_t N> using SSESincDelayLine = sst::basic_blocks::dsp::SSESincDelayLine<N>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
inline float clamp1bp(float value)
{
    return std::clamp(value, -1.f, 1.f);
}
struct EnsembleStorage : EffectGlobal
{
    float samplerate_inv{1.f / 48000};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        samplerate_inv = 1.f / sr;
    }
    static const sst::basic_blocks::tables::SurgeSincTableProvider &sinc()
    {
        static const sst::basic_blocks::tables::SurgeSincTableProvider table;
        return table;
    }
    const float *sinctable = sinc().sinctable;
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
};
union EnsembleValue
{
    float f{};
    int i;
};
struct EnsembleSettings
{
    struct Parameter
    {
        EnsembleValue val;
        bool deactivated{}, temposync{};
    };
    Parameter p[12];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<EnsembleStorage, 8>;
struct EnsembleBase
{
    EnsembleStorage *storage;
    EnsembleSettings *fxdata;
    EnsembleValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    EnsembleBase(EnsembleStorage *s, EnsembleSettings *f, EnsembleValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
} // namespace surge_mm::ensemble
#include "EnsembleHost.h"
