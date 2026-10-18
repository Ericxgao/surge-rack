// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/modulators/FXModControl.h>
#include <chowdsp/shared/chowdsp_DelayLine.h>
#include <chowdsp/shared/SmoothedValue.h>
#include <chowdsp/shared/Oversampling.h>
namespace surge_mm::neuron
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
using ModControl = sst::basic_blocks::modulators::FXModControl<8>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
inline float clamp01(float value)
{
    return std::clamp(value, 0.f, 1.f);
}
struct NeuronStorage : EffectGlobal
{
    double dsamplerate{48000};
    float samplerate_inv{1.f / 48000}, temposyncratio{1};
    void set_samplerate(float sr)
    {
        EffectGlobal::set_samplerate(sr);
        dsamplerate = sr;
        samplerate_inv = 1.f / sr;
    }
};
union NeuronValue
{
    float f{};
    int i;
};
struct NeuronSettings
{
    struct Parameter
    {
        NeuronValue val, val_min, val_max;
        bool deactivated{}, temposync{};
    };
    Parameter p[12];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<NeuronStorage, 8>;
struct NeuronBase
{
    NeuronStorage *storage;
    NeuronSettings *fxdata;
    NeuronValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    NeuronBase(NeuronStorage *s, NeuronSettings *f, NeuronValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (int i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
} // namespace surge_mm::neuron
#include "NeuronHost.h"
