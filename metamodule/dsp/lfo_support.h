// SPDX-License-Identifier: GPL-3.0-or-later
// Storage/parameter subset needed by the eight shapes exposed by Rack LFO.
#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <sst/utilities/globals.h>
#include <sst/basic-blocks/dsp/QuadratureOscillators.h>
#include <sst/basic-blocks/dsp/CorrelatedNoise.h>
#include <sst/basic-blocks/dsp/Interpolators.h>
#include <sst/waveshapers.h>
namespace surge_mm::lfo
{
constexpr int BLOCK_SIZE = 8, BLOCK_SIZE_OS = 16, n_stepseqsteps = 16, mst_lfo = 0;
template <class T> T limit_range(T value, T low, T high)
{
    return std::clamp(value, low, high);
}
enum
{
    lt_sine,
    lt_tri,
    lt_square,
    lt_ramp,
    lt_noise,
    lt_snh,
    lt_envelope,
    lt_stepseq,
    lt_mseg,
    lt_formula
};
enum
{
    lm_freerun,
    lm_keytrigger,
    lm_random
};
enum
{
    type_1,
    type_2,
    type_3,
    type_4
};
inline constexpr std::array<int, 8> deform_counts{3, 3, 1, 3, 1, 2, 3, 2};
union pdata
{
    float f{};
    int i;
    bool b;
};
enum
{
    vt_float,
    vt_int,
    vt_bool
};
struct Parameter
{
    pdata val{}, val_min{}, val_max{};
    int param_id_in_scene{}, valtype{vt_float}, deform_type{};
    bool temposync{}, deactivated{}, extend_range{};
    float get_extended(float f) const
    {
        return extend_range ? 2.f * f - 1.f : f;
    }
    float value_to_normalized(float v) const
    {
        return (v - val_min.f) / (val_max.f - val_min.f);
    }
    void set_value_f01(float v)
    {
        if (valtype == vt_int)
        {
            val.i = std::clamp(int((1 / .99) * (v - .005) * float(val_max.i - val_min.i) + .5) +
                                   val_min.i,
                               val_min.i, val_max.i);
            return;
        }
        if (valtype == vt_bool)
        {
            val.b = v > .5f;
            return;
        }
        val.f = v * (val_max.f - val_min.f) + val_min.f;
        if (temposync)
        {
            float a, b = std::modf(val.f, &a);
            if (b < 0)
            {
                b += 1.f;
                a -= 1.f;
            }
            b = std::pow(2.f, b);
            if (b > 1.41f)
                b = std::log2(double(1.5f));
            else if (b > 1.167f)
                b = std::log2(double(1.3333333333f));
            else
                b = 0;
            val.f = a + b;
        }
        val.f = std::clamp(val.f, val_min.f, val_max.f);
    }
};
struct LFOStorage
{
    Parameter rate, shape, start_phase, magnitude, deform, trigmode, unipolar, delay, hold, attack,
        decay, sustain, release;
    enum
    {
        UNSCALED,
        SCALED
    };
    int lfoExtraAmplitude{UNSCALED};
    LFOStorage()
    {
        Parameter *fields[]{&rate,     &shape,    &start_phase, &magnitude, &deform,
                            &trigmode, &unipolar, &delay,       &hold,      &attack,
                            &decay,    &sustain,  &release};
        constexpr float lo[]{-7, 0, 0, 0, -1, 0, 0, -8, -8, -8, -8, 0, -8};
        constexpr float hi[]{9, 9, 1, 1, 1, 2, 1, 5, 5, 5, 5, 1, 5};
        constexpr float initial[]{0, 0, 0, 1, 0, 1, 0, -8, -8, -8, 0, 1, 5};
        for (unsigned i = 0; i < 13; ++i)
        {
            auto &p = *fields[i];
            p.param_id_in_scene = i;
            p.val_min.f = lo[i];
            p.val_max.f = hi[i];
            p.val.f = initial[i];
        }
        shape.valtype = trigmode.valtype = vt_int;
        shape.val_min.i = 0;
        shape.val_max.i = 9;
        shape.val.i = 0;
        trigmode.val_min.i = 0;
        trigmode.val_max.i = 2;
        trigmode.val.i = lm_keytrigger;
        unipolar.valtype = vt_bool;
        unipolar.val.b = false;
    }
};
struct StepSequencerStorage
{
    float steps[16]{};
    int loop_start{}, loop_end{15};
    uint64_t trigmask{};
};
struct SurgeVoiceState
{
};
// These storage placeholders are only referenced by unreachable desktop modes.
// The native API bounds shape to 0..7; unsupported evaluation aborts explicitly.
[[noreturn]] inline void unsupported_shape()
{
    std::abort();
}
struct MSEGStorage
{
    enum
    {
        ENVELOPE
    };
    int editMode{};
    double totalDuration{};
};
struct FormulaModulatorStorage
{
};
namespace Surge::MSEG
{
struct EvaluatorState
{
    bool released{}, retrigger_FEG{}, retrigger_AEG{};
    void seed(unsigned)
    {
    }
};
inline float valueAt(int, float, float, MSEGStorage *, EvaluatorState *)
{
    unsupported_shape();
}
} // namespace Surge::MSEG
namespace Surge::Formula
{
constexpr int max_formula_outputs = 8;
struct EvaluatorState
{
    bool released{}, isVoice{}, useEnvelope{true}, retrigger_AEG{}, retrigger_FEG{}, raisedError{};
    int activeoutputs{};
    float del{}, a{}, h{}, dec{}, s{}, r{}, rate{}, amp{}, phase{}, deform{}, tempo{}, songpos{};
    std::optional<std::string> error;
};
inline void initEvaluatorState(EvaluatorState &s)
{
    s = {};
}
inline void cleanEvaluatorState(EvaluatorState &s)
{
    s = {};
}
template <class S> void prepareForEvaluation(S *, FormulaModulatorStorage *, EvaluatorState &, bool)
{
    unsupported_shape();
}
template <class S>
void valueAt(int, float, S *, FormulaModulatorStorage *, EvaluatorState *, float *)
{
    unsupported_shape();
}
} // namespace Surge::Formula
struct ModulationSource
{
    float output{};
};
struct SurgeStorage
{
    float samplerate{48000}, temposyncratio{1}, temposyncratio_inv{1};
    double dsamplerate_os_inv{1.0 / 96000}, songpos{};
    std::array<float, 512> table_envrate_linear{};
    // Original engine keeps its own per-voice default_random_engine. This
    // per-module seed source also makes explicit reset deterministic.
    std::mt19937 generator{0x2468ace1};
    SurgeStorage()
    {
        set_samplerate(48000);
    }
    void set_samplerate(float sr)
    {
        samplerate = sr;
        dsamplerate_os_inv = 1.0 / (double(sr) * 2);
        for (int i = 0; i < 512; ++i)
        {
            double k = double(sr) * 2 * std::pow(2.0, ((double(i) - 256.0) / 16.0)) /
                       double(BLOCK_SIZE_OS);
            table_envrate_linear[i] = float(1.f / k);
        }
    }
    int rand()
    {
        return std::uniform_int_distribution<int>(0, RAND_MAX)(generator);
    }
    uint32_t rand_u32()
    {
        return std::uniform_int_distribution<uint32_t>()(generator);
    }
    float rand_01()
    {
        return std::uniform_real_distribution<float>(0, 1)(generator);
    }
    float envelope_rate_linear_nowrap(float);
    float lookup_waveshape_warp(sst::waveshapers::WaveshaperType, float);
    void reportError(const std::string &, const char *)
    {
        unsupported_shape();
    }
};
} // namespace surge_mm::lfo
#include "LFOHost.h"
