// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "filter_tuning.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/Lag.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <sst/filters.h>
#include <sst/filters/HalfRateFilter.h>
#include <sst/waveshapers.h>
namespace surge_mm::resonator
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_OS = 16, BLOCK_SIZE_QUAD = 2, slowrate_m1 = 7;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
inline float clamp1bp(float v)
{
    return std::clamp(v, -1.f, 1.f);
}
inline float amp_to_linear(float x)
{
    x = std::max(0.f, x);
    return x * x * x;
}
struct ResonatorStorage : FilterTuning
{
    double dsamplerate_os{96000};
    void set_samplerate(float sr)
    {
        FilterTuning::set_samplerate(sr);
        dsamplerate_os = 2.0 * sr;
    }
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
};
union ResonatorValue
{
    float f{};
    int i;
};
struct ResonatorSettings
{
    struct Parameter
    {
        ResonatorValue val, val_min, val_max;
        bool extend_range{};
        float get_extended(float x) const
        {
            return x;
        }
    };
    Parameter p[12];
    ResonatorSettings()
    {
        for (unsigned i = 0; i < 12; ++i)
            p[i].val_max.f = 1;
    }
};
struct ResonatorBase
{
    ResonatorStorage *storage;
    ResonatorSettings *fxdata;
    ResonatorValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    ResonatorBase(ResonatorStorage *s, ResonatorSettings *f, ResonatorValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
};
} // namespace surge_mm::resonator
#include "ResonatorHost.h"
