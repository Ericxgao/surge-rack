// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/filters/HalfRateFilter.h>
#include <sst/waveshapers.h>
#include <cassert>
namespace surge_mm::distortion
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2, dist_OS_bits = 2, distortion_OS = 4,
              slowrate_m1 = 7;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
inline float limit01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
struct DistortionStorage : EffectGlobal
{
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
};
union DistortionValue
{
    float f{};
    int i;
};
struct DistortionSettings
{
    struct Parameter
    {
        DistortionValue val;
        bool deactivated{};
        float multiplier{1};
        float get_extended(float x) const
        {
            return x * multiplier;
        }
    };
    Parameter p[12];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<DistortionStorage, 8>;
struct DistortionBase
{
    DistortionStorage *storage;
    DistortionSettings *fxdata;
    DistortionValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    int ringout{};
    DistortionBase(DistortionStorage *s, DistortionSettings *f, DistortionValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
};
inline void initialize_shaper(sst::waveshapers::QuadWaveshaperState &state,
                              sst::waveshapers::WaveshaperType type)
{
    float initial[sst::waveshapers::n_waveshaper_registers]{};
    sst::waveshapers::initializeWaveshaperRegister(type, initial);
    for (unsigned i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
        state.R[i] = _mm_set1_ps(initial[i]);
    state.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());
}
} // namespace surge_mm::distortion
#include "DistortionHost.h"
