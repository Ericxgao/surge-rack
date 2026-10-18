// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <samplerate.h>
#include <clouds/dsp/granular_processor.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace surge_mm::nimbus
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_OS = 16, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T> T limit_range(T x, T lo, T hi)
{
    return std::clamp(x, lo, hi);
}
inline float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
inline float clamp1bp(float x)
{
    return std::clamp(x, -1.f, 1.f);
}
struct NimbusStorage
{
    float samplerate{48000}, samplerate_inv{1.f / 48000};
    void set_samplerate(float sr)
    {
        samplerate = sr;
        samplerate_inv = 1 / sr;
    }
};
union NimbusValue
{
    float f{};
    int i;
};
struct NimbusSettings
{
    struct Parameter
    {
        NimbusValue val;
    };
    Parameter p[12];
};
struct NimbusBase
{
    NimbusStorage *storage;
    NimbusSettings *fxdata;
    NimbusValue *pd;
    float *pd_float[12];
    int *pd_int[12];
    NimbusBase(NimbusStorage *s, NimbusSettings *f, NimbusValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 12; ++i)
        {
            pd_float[i] = &v[i].f;
            pd_int[i] = &v[i].i;
        }
    }
};
} // namespace surge_mm::nimbus
