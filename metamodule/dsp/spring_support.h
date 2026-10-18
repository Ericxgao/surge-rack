// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sst/utilities/globals.h>
#include "effect_config.h"
#include <sst/basic-blocks/dsp/BlockInterpolators.h>
#include <sst/basic-blocks/dsp/FastMath.h>
#include <sst/basic-blocks/mechanics/block-ops.h>
#include <chowdsp/shared/chowdsp_DelayLine.h>
#include <chowdsp/shared/SmoothedValue.h>
#include <array>
#include <random>
namespace surge_mm::spring
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_QUAD = 2;
using lipol_ps_blocksz = sst::basic_blocks::dsp::lipol_sse<8, false>;
using chowdsp::DelayLine;
using chowdsp::SmoothedValue;
namespace DelayLineInterpolationTypes = chowdsp::DelayLineInterpolationTypes;
namespace ValueSmoothingTypes = chowdsp::ValueSmoothingTypes;
namespace mech = sst::basic_blocks::mechanics;
inline float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
inline double dB_to_linear(double x)
{
    return std::pow(10.0, .05 * x);
}
inline float vSum(__m128 x)
{
    auto a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    a = _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    _mm_store_ss(&f, a);
    return f;
}
// Four lanes with JUCE's arithmetic but no desktop JUCE dependency. Alignment
// stays at float alignment so the shared byte-backed Chow buffer is sufficient.
struct SpringVector
{
    using value_type = float;
    // A native register avoids GCC's ARM aggregate copies through scalar
    // registers and the stack for every operator. The delay's byte storage
    // guarantees float alignment only; retain unaligned-safe vector accesses.
    typedef __m128 Register __attribute__((aligned(4)));
    Register value;
    SpringVector() : value(_mm_setzero_ps())
    {
    }
    SpringVector(float x) : value(_mm_set1_ps(x))
    {
    }
    explicit SpringVector(__m128 x) : value(x)
    {
    }
    static SpringVector fromRawArray(const float *p)
    {
        return SpringVector(_mm_loadu_ps(p));
    }
    void copyToRawArray(float *p) const
    {
        _mm_storeu_ps(p, value);
    }
    float &operator[](size_t i)
    {
        return value[i];
    }
    SpringVector &operator+=(SpringVector b)
    {
        *this = *this + b;
        return *this;
    }
    friend SpringVector operator+(SpringVector a, SpringVector b)
    {
        return SpringVector(_mm_add_ps(a.value, b.value));
    }
    friend SpringVector operator-(SpringVector a, SpringVector b)
    {
        return SpringVector(_mm_sub_ps(a.value, b.value));
    }
    friend SpringVector operator*(SpringVector a, SpringVector b)
    {
        return SpringVector(_mm_mul_ps(a.value, b.value));
    }
};
static_assert(sizeof(SpringVector) == 16 && alignof(SpringVector) == alignof(float));
// Prepare reflection interpolation at control rate. Retain Chow's full ring,
// pointer convention and arithmetic order, including zero-delay history.
class SpringReflectionDelay : public DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd>
{
    using Base = DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd>;

  public:
    using Base::Base;
    struct Tap
    {
        int offset{};
        float fraction{}, c1{}, c2{}, c3{}, c4{};
    };
    static Tap makeTap(float samples)
    {
        Tap t;
        samples = std::clamp(samples, 0.f, float(1 << 18));
        t.offset = int(std::floor(samples));
        t.fraction = samples - float(t.offset);
        if (t.offset >= 1)
        {
            ++t.fraction;
            --t.offset;
        }
        const auto d1 = t.fraction - 1.f, d2 = t.fraction - 2.f, d3 = t.fraction - 3.f;
        t.c1 = -d1 * d2 * d3 / 6.f;
        t.c2 = d2 * d3 * .5f;
        t.c3 = -d1 * d3 * .5f;
        t.c4 = d1 * d2 / 6.f;
        return t;
    }
    float readTap(int channel, const Tap &t) const noexcept
    {
        const auto *p =
            this->bufferData.getChannelPointer(channel) + this->readPos[channel] + t.offset;
        return p[0] * t.c1 + t.fraction * (p[1] * t.c2 + p[2] * t.c3 + p[3] * t.c4);
    }
};
// Bounds include the maximum parameter values at the highest accepted rate.
static_assert((1000 + .099 * 192000) * 1.07 < 32768);
// Reflection taps retain the original full ring, with one shared history.
static_assert(.00335 * 192000 < 1024);
struct SpringStorage : EffectGlobal
{
};
union SpringValue
{
    float f{};
    int i;
};
struct SpringSettings
{
};
struct SpringBase
{
    SpringStorage *storage;
    SpringSettings *fxdata;
    SpringValue *pd;
    float *pd_float[8];
    SpringBase(SpringStorage *s, SpringSettings *f, SpringValue *v) : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 8; ++i)
            pd_float[i] = &v[i].f;
    }
};
} // namespace surge_mm::spring
