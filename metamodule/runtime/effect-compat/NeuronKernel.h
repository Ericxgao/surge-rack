/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#pragma once
#include "neuron_support.h"
namespace surge_mm::neuron
{
using namespace surge_mm::neuron;
using namespace chowdsp;
class NeuronKernel : public NeuronBase
{
  public:
    enum neuron_params
    {
        neuron_drive_wh = 0,
        neuron_squash_wf,
        neuron_stab_uf,
        neuron_asym_uh,
        neuron_bias_bf,

        neuron_comb_freq,
        neuron_comb_sep,

        neuron_lfo_wave,
        neuron_lfo_rate,
        neuron_lfo_depth,

        neuron_width,
        neuron_gain,

        neuron_num_params,
    };

    NeuronKernel(NeuronStorage *storage, NeuronSettings *fxdata, NeuronValue *pd);
    ~NeuronKernel();

    const char *get_effectname()
    {
        return "Neuron";
    }

    void init();
    void process(float *dataL, float *dataR);
    void suspend();

    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);

  public:
    void reset_native()
    {
        Wf.setCurrentAndTargetValue(.5f);
        Wh.setCurrentAndTargetValue(.5f);
        Uf.setCurrentAndTargetValue(.5f);
        Uh.setCurrentAndTargetValue(.5f);
        bf.setCurrentAndTargetValue(0);
        delay1Smooth.setCurrentAndTargetValue(0);
        delay2Smooth.setCurrentAndTargetValue(0);
        makeup.set_target(0);
        makeup.instantize();
        outgain.set_target(0);
        outgain.instantize();
        width.set_target(0);
        width.instantize();
        modLFO.resetState();
        modLFO.setSampleRate(storage->samplerate);
        init();
    }
    void set_params();
    void process_internal(float *dataL, float *dataR, const int numSamples);

    inline float processSample(float x, float yPrev) noexcept
    {
        float f = sigmoid(Wf.getNextValue() * x + Uf.getNextValue() * yPrev + bf.getNextValue());
        return f * yPrev +
               (1.0f - f) * std::tanh(Wh.getNextValue() * x + Uh.getNextValue() * f * yPrev);
    }

    inline float sigmoid(float x) const noexcept
    {
        return 1.0f / (1.0f + std::exp(-x));
    }

    enum
    {
        numSteps = 200,
    };

    SmoothedValue<float, ValueSmoothingTypes::Linear> Wf = 0.5f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> Wh = 0.5f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> Uf = 0.5f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> Uh = 0.5f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> bf = 0.0f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> delay1Smooth = 0.0f;
    SmoothedValue<float, ValueSmoothingTypes::Linear> delay2Smooth = 0.0f;

    float y1[2] = {0.0f, 0.0f};

    BiquadFilter dc_blocker;
    lipol_ps_blocksz makeup alignas(16), width alignas(16), outgain alignas(16);
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Linear> delay1{1 << 18};
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Linear> delay2{1 << 18};
    Oversampling<2, BLOCK_SIZE> os;

    ModControl modLFO;
};

inline NeuronKernel::NeuronKernel(NeuronStorage *storage, NeuronSettings *fxdata, NeuronValue *pd)
    : NeuronBase(storage, fxdata, pd), modLFO(storage->samplerate, storage->samplerate_inv)
{
    dc_blocker.setBlockSize(BLOCK_SIZE);
    makeup.set_blocksize(BLOCK_SIZE);
    outgain.set_blocksize(BLOCK_SIZE);
}

inline NeuronKernel::~NeuronKernel()
{
}

inline void NeuronKernel::init()
{
    Wf.reset(numSteps);
    Wh.reset(numSteps);
    Uf.reset(numSteps);
    Uh.reset(numSteps);
    bf.reset(numSteps);

    delay1Smooth.reset(numSteps);
    delay2Smooth.reset(numSteps);

    os.reset();

    delay1.prepare({storage->dsamplerate * os.getOSRatio(), BLOCK_SIZE, 1});
    delay2.prepare({storage->dsamplerate * os.getOSRatio(), BLOCK_SIZE, 1});
    delay1.setDelay(0.0f);
    delay2.setDelay(0.0f);

    y1[0] = 0.0f;
    y1[1] = 0.0f;

    dc_blocker.suspend();
    dc_blocker.coeff_HP(35.0f / storage->samplerate, 0.707);
    dc_blocker.coeff_instantize();

    width.instantize();

    makeup.set_target(1.0f);
    outgain.set_target(0.0f);
}

inline void NeuronKernel::process(float *dataL, float *dataR)
{
    set_params();

    os.upsample(dataL, dataR);
    process_internal(os.leftUp, os.rightUp, os.getUpBlockSize());
    os.downsample(dataL, dataR);

    dc_blocker.process_block(dataL, dataR);
    makeup.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];

    applyWidth(dataL, dataR, width);
    outgain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
    modLFO.post_process();
}

inline void NeuronKernel::process_internal(float *dataL, float *dataR, const int numSamples)
{
    for (int k = 0; k < numSamples; k++)
    {
        dataL[k] = processSample(dataL[k], y1[0]);
        dataR[k] = processSample(dataR[k], y1[1]);

        auto lfo_val = 1.0f + 0.5f * modLFO.value();
        delay1.setDelay(delay1Smooth.getNextValue() * lfo_val);
        delay2.setDelay(delay2Smooth.getNextValue() * lfo_val);
        delay1.pushSample(0, dataL[k]);
        delay2.pushSample(0, dataR[k]);

        y1[0] = delay1.popSample(0);
        y1[1] = delay2.popSample(0);
    }
}

inline void NeuronKernel::set_params()
{
    auto bf_clamped = clamp01(*pd_float[neuron_bias_bf]);
    auto wh_clamped = clamp01(*pd_float[neuron_drive_wh]);

    Wf.setTargetValue(clamp01(*pd_float[neuron_squash_wf]) * 20.0f);
    Wh.setTargetValue(storage->db_to_linear(wh_clamped));
    Uf.setTargetValue(clamp01(*pd_float[neuron_stab_uf]) * 5.0f);
    Uh.setTargetValue(clamp01(*pd_float[neuron_asym_uh]) * 0.9f);
    bf.setTargetValue(bf_clamped * 6.0f - 1.0f);

    // tune delay length
    auto freqHz1 = (2 * 3.14159265358979323846) * 440 *
                   storage->note_to_pitch_ignoring_tuning(*pd_float[neuron_comb_freq]);
    auto freqHz2 = (2 * 3.14159265358979323846) * 440 *
                   storage->note_to_pitch_ignoring_tuning(*pd_float[neuron_comb_freq] +
                                                          *pd_float[neuron_comb_sep]);
    auto delayTimeSec1 = 1.0f / (float)freqHz1;
    auto delayTimeSec2 = 1.0f / (float)freqHz2;

    delay1Smooth.setTargetValue(delayTimeSec1 * 0.5f * storage->samplerate * os.getOSRatio());
    delay2Smooth.setTargetValue(delayTimeSec2 * 0.5f * storage->samplerate * os.getOSRatio());

    // modulation settings
    int mwave = *pd_int[neuron_lfo_wave];
    float rate =
        storage->envelope_rate_linear(-limit_range(*pd_float[neuron_lfo_rate], -8.f, 10.f)) *
        (fxdata->p[neuron_lfo_rate].temposync ? storage->temposyncratio : 1.f);
    float depth_val = limit_range(*pd_float[neuron_lfo_depth], 0.f, 2.f);

    if (fxdata->p[neuron_lfo_rate].deactivated)
    {
        auto rmin = fxdata->p[neuron_lfo_rate].val_min.f;
        auto rmax = fxdata->p[neuron_lfo_rate].val_max.f;
        auto phase = clamp01((*pd_float[neuron_lfo_rate] - rmin) / (rmax - rmin));

        modLFO.pre_process(mwave, 0.f, depth_val, phase);
    }
    else
    {
        modLFO.pre_process(mwave, rate, depth_val, 0.f);
    }

    // calc makeup gain
    auto drive_makeup = [](float wh) -> float { return std::exp(-0.11898f * wh) + 1.0f; };

    auto bias_makeup = [](float bf) -> float { return 6.0f * std::pow(bf, 7.5f) + 0.9f; };

    const auto makeupGain = drive_makeup(wh_clamped) * bias_makeup(bf_clamped);

    makeup.set_target_smoothed(makeupGain);

    width.set_target_smoothed(storage->db_to_linear(*pd_float[neuron_width]));
    outgain.set_target_smoothed(storage->db_to_linear(*pd_float[neuron_gain]));
}

inline void NeuronKernel::suspend()
{
    init();
}

inline void NeuronKernel::init_default_values()
{
    fxdata->p[neuron_drive_wh].val.f = 0.0f;
    fxdata->p[neuron_squash_wf].val.f = 0.5f;
    fxdata->p[neuron_stab_uf].val.f = 0.5f;
    fxdata->p[neuron_asym_uh].val.f = 1.0f;
    fxdata->p[neuron_bias_bf].val.f = 0.0f;

    fxdata->p[neuron_comb_freq].val.f = 70.0f;
    fxdata->p[neuron_comb_sep].val.f = 0.5f;

    fxdata->p[neuron_lfo_wave].val.i = 0;
    fxdata->p[neuron_lfo_rate].val.f = -2.f;
    fxdata->p[neuron_lfo_rate].deactivated = false;
    fxdata->p[neuron_lfo_depth].val.f = 0.f;

    fxdata->p[neuron_width].val.f = 0.0f;
    fxdata->p[neuron_gain].val.f = 0.0f;
}
} // namespace surge_mm::neuron
