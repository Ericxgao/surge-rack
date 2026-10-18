// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/MidSide.h>
#include <sst/basic-blocks/dsp/QuadratureOscillators.h>
#include <cstring>
#include <new>
#include <memory>
#include <array>
namespace surge_mm::reverb2
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
constexpr float BLOCK_SIZE_INV = 1.f / 8;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
template <class T, bool first = true> using lipol = sst::basic_blocks::dsp::lipol<T, 8, first>;
template <class T> T limit_range(T x, T lo, T hi)
{
    return std::clamp(x, lo, hi);
}
struct Reverb2Storage : EffectGlobal
{
    float temposyncratio_inv{1};
};
union Reverb2Value
{
    float f{};
    int i;
};
struct Reverb2Settings
{
    struct Parameter
    {
        Reverb2Value val;
        bool temposync{};
    };
    Parameter p[10];
};
struct Reverb2Base
{
    Reverb2Storage *storage;
    Reverb2Settings *fxdata;
    Reverb2Value *pd;
    float *pd_float[10];
    Reverb2Base(Reverb2Storage *s, Reverb2Settings *f, Reverb2Value *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 10; ++i)
            pd_float[i] = &v[i].f;
    }
    void applyWidth(float *__restrict, float *__restrict, lipol_ps_blocksz &);
};
static_assert(.0605 * 2 * 192000 < 32768);
static_assert((.1788 * 2 + .005) * 192000 < 131072);
} // namespace surge_mm::reverb2
#include "Reverb2Host.h"
