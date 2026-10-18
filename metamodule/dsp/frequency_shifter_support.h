// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/Lag.h>
#include <sst/basic-blocks/dsp/QuadratureOscillators.h>
#include <sst/basic-blocks/tables/SincTableProvider.h>
#include <sst/filters/HalfRateFilter.h>
#include <sst/waveshapers.h>
namespace surge_mm::frequency_shifter
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2, max_delay_length = 1 << 18, FIRipol_N = 12,
              FIRipol_M = 256, FIRoffset = 6;
constexpr float BLOCK_SIZE_INV = 1.f / 8;
template <class T, bool first = true> using lipol = sst::basic_blocks::dsp::lipol<T, 8, first>;
template <class T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T value, T lo, T hi)
{
    return std::clamp(value, lo, hi);
}
struct FrequencyShifterStorage : EffectGlobal
{
    float temposyncratio_inv{1};
    static const sst::basic_blocks::tables::SurgeSincTableProvider &sinc()
    {
        static const sst::basic_blocks::tables::SurgeSincTableProvider table;
        return table;
    }
    const float *sinctable1X = sinc().sinctable1X;
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
};
union FrequencyShifterValue
{
    float f{};
    int i;
};
struct FrequencyShifterSettings
{
    struct Parameter
    {
        FrequencyShifterValue val, val_min;
        bool temposync{}, extend_range{};
    };
    Parameter p[5];
    FrequencyShifterSettings()
    {
        p[2].val_min.f = -8;
    }
};
struct FrequencyShifterBase
{
    FrequencyShifterStorage *storage;
    FrequencyShifterSettings *fxdata;
    FrequencyShifterValue *pd;
    float *pd_float[5];
    int ringout{};
    FrequencyShifterBase(FrequencyShifterStorage *s, FrequencyShifterSettings *f,
                         FrequencyShifterValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 5; ++i)
            pd_float[i] = &v[i].f;
    }
};
} // namespace surge_mm::frequency_shifter
#include "FrequencyShifterHost.h"
