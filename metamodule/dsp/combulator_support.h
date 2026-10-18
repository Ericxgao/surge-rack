// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "resonator_support.h"
#include <sst/filters/BiquadFilter.h>
#include <sst/basic-blocks/dsp/CorrelatedNoise.h>
#include <random>
namespace surge_mm::combulator
{
using namespace resonator;
constexpr int MAX_FB_COMB_EXTENDED = sst::filters::utilities::MAX_FB_COMB_EXTENDED, FIRipol_N = 12;
inline float clamp01(float x)
{
    return std::clamp(x, 0.f, 1.f);
}
struct CombulatorStorage : ResonatorStorage
{
    double dsamplerate_inv{1.0 / 48000};
    // Same generator and distribution as SurgeStorage, owned by each audio lane.
    std::minstd_rand random{1};
    std::uniform_real_distribution<float> distribution{-1, 1};
    unsigned seed{1};
    float (*rack_random)(void *){};
    void *rack_random_context{};
    void set_samplerate(float sr)
    {
        ResonatorStorage::set_samplerate(sr);
        dsamplerate_inv = 1.0 / sr;
    }
    float rand_pm1()
    {
        return rack_random ? rack_random(rack_random_context) : distribution(random);
    }
    void reset_random()
    {
        random.seed(seed);
        distribution.reset();
    }
    static float db_to_linear(float x)
    {
        return std::pow(10.f, .05f * x);
    }
};
using CombulatorValue = ResonatorValue;
struct CombulatorSettings
{
    struct Parameter
    {
        CombulatorValue val;
        bool extend_range{}, deactivated{};
    };
    Parameter p[12];
};
using BiquadFilter = sst::filters::Biquad::BiquadFilter<CombulatorStorage, 8>;
struct CombulatorBase
{
    CombulatorStorage *storage;
    CombulatorSettings *fxdata;
    CombulatorValue *pd;
    float *pd_float[12];
    CombulatorBase(CombulatorStorage *s, CombulatorSettings *f, CombulatorValue *v)
        : storage(s), fxdata(f), pd(v)
    {
        for (unsigned i = 0; i < 12; ++i)
            pd_float[i] = &v[i].f;
    }
};
} // namespace surge_mm::combulator
