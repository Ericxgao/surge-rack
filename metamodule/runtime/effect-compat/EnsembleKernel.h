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
#include "ensemble_support.h"
namespace surge_mm::ensemble
{
using namespace surge_mm::ensemble;
using namespace sst::waveshapers;
namespace mech = sst::basic_blocks::mechanics;
class EnsembleKernel : public EnsembleBase
{
    lipol_ps_blocksz width alignas(16), mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    void reset_native()
    {
        delL.clear();
        delR.clear();
        for (auto &bank : modlfos)
            for (auto &lfo : bank)
                lfo.resetState();
        sampleRateReset();
        init();
    }
    enum ens_params
    {
        ens_input_filter = 0,

        ens_lfo_freq1,
        ens_lfo_depth1,
        ens_lfo_freq2,
        ens_lfo_depth2,

        ens_delay_type,
        ens_delay_clockrate,
        ens_delay_sat,
        ens_delay_feedback,

        ens_width,
        ens_mix,

        ens_output_filter,

        ens_num_ctrls,
    };

    enum EnsembleStages
    {
        ens_sinc = 0,
        ens_128,
        ens_256,
        ens_512,
        ens_1024,
        ens_2048,
        ens_4096,
    };

    EnsembleKernel(EnsembleStorage *storage, EnsembleSettings *fxdata, EnsembleValue *pd);
    ~EnsembleKernel();
    const char *get_effectname()
    {
        return "Ensemble";
    }
    void init();
    void sampleRateReset();
    void process(float *dataL, float *dataR);
    void suspend();
    void setvars(bool init);
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);
    void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision);

  private:
    float getFeedbackGain(bool bbd) const noexcept;
    void process_sinc_delays(float *dataL, float *dataR, float delayCenterMs, float delayScale);

    ModControl modlfos[2][3]; // 2 LFOs differening by 120 degree in phase at outputs
    SSESincDelayLine<8192> delL, delR;
    BBDDelayLine<128> del_128L1, del_128L2, del_128R1, del_128R2;
    BBDDelayLine<256> del_256L1, del_256L2, del_256R1, del_256R2;
    BBDDelayLine<512> del_512L1, del_512L2, del_512R1, del_512R2;
    BBDDelayLine<1024> del_1024L1, del_1024L2, del_1024R1, del_1024R2;
    BBDDelayLine<2048> del_2048L1, del_2048L2, del_2048R1, del_2048R2;
    BBDDelayLine<4096> del_4096L1, del_4096L2, del_4096R1, del_4096R2;

    BBDNonlin bbd_saturation_sse;
    size_t block_counter;

    float fbStateL, fbStateR;
    BiquadFilter dc_blocker[2];
    BiquadFilter reconstrFilter[2];
    BiquadFilter sincInputFilter;
};

inline int ensemble_num_stages(int i)
{
    switch (i)
    {
    case EnsembleKernel::ens_sinc:
        return 512; // faking it :)!
    case EnsembleKernel::ens_128:
        return 128;
    case EnsembleKernel::ens_256:
        return 256;
    case EnsembleKernel::ens_512:
        return 512;
    case EnsembleKernel::ens_1024:
        return 1024;
    case EnsembleKernel::ens_2048:
        return 2048;
    case EnsembleKernel::ens_4096:
        return 4096;
    }
    return 1;
}
inline float calculateFilterParamFrequency(float *fval, EnsembleStorage *storage)
{
    auto param_val = fval[EnsembleKernel::ens_input_filter];
    auto clock_rate = std::max(fval[EnsembleKernel::ens_delay_clockrate], 0.01f);
    auto freq = 2.0f * (float)M_PI * 400.f * storage->note_to_pitch_ignoring_tuning(param_val);
    auto freq_adjust = freq * std::pow(clock_rate * 0.01f, 0.75f);
    return std::min(freq_adjust, 25000.0f);
}
namespace
{
constexpr float delay1Ms = 0.6f;
constexpr float delay2Ms = 0.2f;
constexpr float qVals2ndOrderButter[] = {1.0 / 0.7654, 1.0 / 1.8478};
constexpr float defaultFilterCut{45.2326448f}; // 6 kHz (when added to MIDI note 69 = A440)
} // namespace
inline EnsembleKernel::EnsembleKernel(EnsembleStorage *storage, EnsembleSettings *fxdata,
                                      EnsembleValue *pd)
    : EnsembleBase(storage, fxdata, pd), delL(storage->sinctable), delR(storage->sinctable),
      dc_blocker{storage, storage}, reconstrFilter{storage, storage}, sincInputFilter(storage)
{
    width.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);

    EnsembleKernel::sampleRateReset();

    for (int i = 0; i < 2; ++i)
    {
        dc_blocker[i].setBlockSize(BLOCK_SIZE);
        reconstrFilter[i].setBlockSize(BLOCK_SIZE);
    }
}

inline EnsembleKernel::~EnsembleKernel()
{
}

inline void EnsembleKernel::init()
{
    setvars(true);
    block_counter = 0;

    auto reset_bbds = [this](auto &delL1, auto &delL2, auto &delR1, auto &delR2) {
        for (auto *del : {&delL1, &delL2, &delR1, &delR2})
        {
            del->setFilterFreq(10000.0f);
            del->setDelayTime(0.005f);
        }
    };

    reset_bbds(del_128L1, del_128L2, del_128R1, del_128R2);
    reset_bbds(del_256L1, del_256L2, del_256R1, del_256R2);
    reset_bbds(del_512L1, del_512L2, del_512R1, del_512R2);
    reset_bbds(del_1024L1, del_1024L2, del_1024R1, del_1024R2);
    reset_bbds(del_2048L1, del_2048L2, del_2048R1, del_2048R2);
    reset_bbds(del_4096L1, del_4096L2, del_4096R1, del_4096R2);

    bbd_saturation_sse.setDrive(0.5f);

    fbStateL = 0.0f;
    fbStateR = 0.0f;

    // Butterworth highpass and output correction filter
    for (int i = 0; i < 2; ++i)
    {
        dc_blocker[i].suspend();
        reconstrFilter[i].suspend();
    }

    sincInputFilter.suspend();
}

inline void EnsembleKernel::setvars(bool init)
{
    if (init)
    {
        // Butterworth highpass and output correction filter
        for (int i = 0; i < 2; ++i)
        {
            dc_blocker[i].coeff_HP(dc_blocker[i].calc_omega_from_Hz(50.0), qVals2ndOrderButter[i]);
            reconstrFilter[i].coeff_LP(
                reconstrFilter[i].calc_omega(fxdata->p[ens_output_filter].val.f / 12.f), 0.7071);
        }

        sincInputFilter.coeff_LP(sincInputFilter.calc_omega_from_Hz(20000.0), 0.7071);

        width.set_target(0.f);
        mix.set_target(1.f);

        width.instantize();
        mix.instantize();
    }
    else
    {
        // Butterworth highpass and output correction filter
        for (int i = 0; i < 2; ++i)
        {
            dc_blocker[i].coeff_HP(dc_blocker[i].calc_omega_from_Hz(50.0), qVals2ndOrderButter[i]);
            dc_blocker[i].coeff_instantize();

            reconstrFilter[i].coeff_LP(
                reconstrFilter[i].calc_omega(*pd_float[ens_output_filter] / 12.f), 0.7071);
            reconstrFilter[i].coeff_instantize();
        }

        sincInputFilter.coeff_LP(sincInputFilter.calc_omega_from_Hz(20000.0), 0.7071);
        sincInputFilter.coeff_instantize();
    }
}

inline void EnsembleKernel::sampleRateReset()
{
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            modlfos[i][j].samplerate = storage->samplerate;
            modlfos[i][j].samplerate_inv = storage->samplerate_inv;
        }
    }

    bbd_saturation_sse.reset(storage->samplerate);

    auto init_bbds = [this](auto &delL1, auto &delL2, auto &delR1, auto &delR2) {
        for (auto *del : {&delL1, &delL2, &delR1, &delR2})
            del->prepare(storage->samplerate);
    };

    init_bbds(del_128L1, del_128L2, del_128R1, del_128R2);
    init_bbds(del_256L1, del_256L2, del_256R1, del_256R2);
    init_bbds(del_512L1, del_512L2, del_512R1, del_512R2);
    init_bbds(del_1024L1, del_1024L2, del_1024R1, del_1024R2);
    init_bbds(del_2048L1, del_2048L2, del_2048R1, del_2048R2);
    init_bbds(del_4096L1, del_4096L2, del_4096R1, del_4096R2);
}

inline float EnsembleKernel::getFeedbackGain(bool bbd) const noexcept
{
    // normally we would just need the feedback to be less than 1
    // however here we're adding the output of 2 delay lines so
    // we need the feedback to be less than 0.5.
    auto baseFeedbackParam = (bbd ? 0.2f : 1.0f) * *pd_float[ens_delay_feedback];
    return 0.49f * std::pow(baseFeedbackParam, 0.5f);
}

inline void EnsembleKernel::process_sinc_delays(float *dataL, float *dataR, float delayCenterMs,
                                                float delayScale)
{
    // copy input data ("dry") to processed output ("wet)
    mech::copy_from_to<BLOCK_SIZE>(dataL, L);
    mech::copy_from_to<BLOCK_SIZE>(dataR, R);

    const auto aa_cutoff = calculateFilterParamFrequency(*pd_float, storage);

    sincInputFilter.coeff_LP(2 * M_PI * aa_cutoff / storage->samplerate, 0.7071);
    sincInputFilter.process_block(L, R);

    float del1 = delayScale * delay1Ms * 0.001 * storage->samplerate;
    float del2 = delayScale * delay2Ms * 0.001 * storage->samplerate;
    float del0 = delayCenterMs * 0.001 * storage->samplerate;

    bbd_saturation_sse.setDrive(*pd_float[ens_delay_sat]);

    float fbGain = getFeedbackGain(false);

    for (int s = 0; s < BLOCK_SIZE; ++s)
    {
        // reduce input by 3 dB
        L[s] *= 0.75f;
        R[s] *= 0.75f;

        // soft-clip input
        L[s] = storage->lookup_waveshape(WaveshaperType::wst_soft, L[s] + fbStateL);
        R[s] = storage->lookup_waveshape(WaveshaperType::wst_soft, R[s] + fbStateR);

        delL.write(L[s]);
        delR.write(R[s]);

        // OK so look at the diagram in #3743
        float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
        float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
        float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

        float ltap1 = t1;
        float ltap2 = t2;
        float rtap1 = t2;
        float rtap2 = t3;

        float delayOuts alignas(16)[4];
        delayOuts[0] = delL.read(ltap1);
        delayOuts[1] = delL.read(ltap2);
        delayOuts[2] = delR.read(rtap1);
        delayOuts[3] = delR.read(rtap2);

        fbStateL = fbGain * (delayOuts[0] + delayOuts[1]);
        fbStateR = fbGain * (delayOuts[2] + delayOuts[3]);

        // avoid DC build-up in the feedback path
        dc_blocker[0].process_sample_nolag(fbStateL, fbStateR);
        dc_blocker[1].process_sample_nolag(fbStateL, fbStateR);

        auto delayOutsVec = vLoad(delayOuts);
        auto waveshaperOutsVec = bbd_saturation_sse.processSample(delayOutsVec);

        float waveshaperOuts alignas(16)[4];

        _mm_store_ps(waveshaperOuts, waveshaperOutsVec);

        L[s] = waveshaperOuts[0] + waveshaperOuts[1];
        R[s] = waveshaperOuts[2] + waveshaperOuts[3];

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                modlfos[j][i].post_process();
            }
        }
    }
}

inline void EnsembleKernel::process(float *dataL, float *dataR)
{
    setvars(false);

    // limit between 0.005 and 40 Hz with modulation
    double rate1 =
        storage->envelope_rate_linear(-limit_range(*pd_float[ens_lfo_freq1], -7.64386f, 5.322f));
    double rate2 =
        storage->envelope_rate_linear(-limit_range(*pd_float[ens_lfo_freq2], -7.64386f, 5.322f));
    float roff = 0;
    constexpr float onethird = 1.0 / 3.0;

    for (int i = 0; i < 3; ++i)
    {
        modlfos[0][i].pre_process(ModControl::mod_sine, rate1, *pd_float[ens_lfo_depth1], roff);
        modlfos[1][i].pre_process(ModControl::mod_sine, rate2, *pd_float[ens_lfo_depth2], roff);

        roff += onethird;
    }

    auto bbd_stages = *pd_int[ens_delay_type];
    const auto clock_rate =
        std::max(*pd_float[ens_delay_clockrate], 1.0f); // make sure this is not zero!
    const auto numStages = ensemble_num_stages(bbd_stages);
    const auto delayCenterMs = (float)numStages / (2.0f * clock_rate);
    const auto delayScale =
        0.95f * delayCenterMs / (delay1Ms + delay2Ms); // make sure total delay is always positive

    auto process_bbd_delays = [=](float *dataL, float *dataR, auto &delL1, auto &delL2, auto &delR1,
                                  auto &delR2) {
        // copy input data ("dry") to processed output ("wet)
        mech::copy_from_to<BLOCK_SIZE>(dataL, L);
        mech::copy_from_to<BLOCK_SIZE>(dataR, R);

        // setting the filter frequency takes a while, so
        // let's only do it every 4 times
        if (block_counter++ == 3)
        {
            const auto aa_cutoff = calculateFilterParamFrequency(*pd_float, storage);

            for (auto *del : {&delL1, &delL2, &delR1, &delR2})
            {
                del->setFilterFreq(aa_cutoff);
            }

            block_counter = 0;
        }

        bbd_saturation_sse.setDrive(*pd_float[ens_delay_sat]);

        float fbGain = getFeedbackGain(true);
        float del1 = delayScale * delay1Ms * 0.001;
        float del2 = delayScale * delay2Ms * 0.001;
        float del0 = delayCenterMs * 0.001;

        for (int s = 0; s < BLOCK_SIZE; ++s)
        {
            // reduce input by 3 dB
            L[s] *= 0.75f;
            R[s] *= 0.75f;

            // soft-clip input
            L[s] = storage->lookup_waveshape(WaveshaperType::wst_soft, L[s] + fbStateL);
            R[s] = storage->lookup_waveshape(WaveshaperType::wst_soft, R[s] + fbStateR);

            // OK so look at the diagram in #3743
            float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
            float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
            float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

            delL1.setDelayTime(t1);
            delL2.setDelayTime(t2);
            delR1.setDelayTime(t2);
            delR2.setDelayTime(t3);

            float delayOuts alignas(16)[4];

            delayOuts[0] = delL1.process(L[s]);
            delayOuts[1] = delL2.process(L[s]);
            delayOuts[2] = delR1.process(R[s]);
            delayOuts[3] = delR2.process(R[s]);

            fbStateL = fbGain * (delayOuts[0] + delayOuts[1]);
            fbStateR = fbGain * (delayOuts[2] + delayOuts[3]);

            // avoid DC build-up in the feedback path
            dc_blocker[0].process_sample_nolag(fbStateL, fbStateR);
            dc_blocker[1].process_sample_nolag(fbStateL, fbStateR);

            auto delayOutsVec = vLoad(delayOuts);
            auto waveshaperOutsVec = bbd_saturation_sse.processSample(delayOutsVec);

            float waveshaperOuts alignas(16)[4];

            _mm_store_ps(waveshaperOuts, waveshaperOutsVec);

            L[s] = waveshaperOuts[0] + waveshaperOuts[1];
            R[s] = waveshaperOuts[2] + waveshaperOuts[3];

            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 2; ++j)
                {
                    modlfos[j][i].post_process();
                }
            }
        }

        mech::mul_block<BLOCK_SIZE>(L, storage->db_to_linear(-8.0f));
        mech::mul_block<BLOCK_SIZE>(R, storage->db_to_linear(-8.0f));
    };

    switch (bbd_stages)
    {
    case ens_sinc:
        process_sinc_delays(dataL, dataR, delayCenterMs, delayScale);
        break;
    case ens_128:
        process_bbd_delays(dataL, dataR, del_128L1, del_128L2, del_128R1, del_128R2);
        break;
    case ens_256:
        process_bbd_delays(dataL, dataR, del_256L1, del_256L2, del_256R1, del_256R2);
        break;
    case ens_512:
        process_bbd_delays(dataL, dataR, del_512L1, del_512L2, del_512R1, del_512R2);
        break;
    case ens_1024:
        process_bbd_delays(dataL, dataR, del_1024L1, del_1024L2, del_1024R1, del_1024R2);
        break;
    case ens_2048:
        process_bbd_delays(dataL, dataR, del_2048L1, del_2048L2, del_2048R1, del_2048R2);
        break;
    case ens_4096:
        process_bbd_delays(dataL, dataR, del_4096L1, del_4096L2, del_4096R1, del_4096R2);
        break;
    }

    // output correction filter
    if (!fxdata->p[ens_output_filter].deactivated)
    {
        for (int i = 0; i < 2; i++)
        {
            reconstrFilter[i].coeff_LP(
                reconstrFilter[i].calc_omega(*pd_float[ens_output_filter] / 12.f), 0.7071);
            reconstrFilter[i].process_block(L, R);
        }
    }

    // scale width
    width.set_target_smoothed(clamp1bp(*pd_float[ens_width]));

    applyWidth(L, R, width);

    mix.set_target_smoothed(clamp1bp(*pd_float[ens_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

inline void EnsembleKernel::suspend()
{
    init();
}

inline void EnsembleKernel::init_default_values()
{
    fxdata->p[ens_input_filter].val.f = defaultFilterCut;

    // we want LFO 1 at .18 Hz and LFO 2 at 5.52 Hz
    // these go as 2^x so...
    fxdata->p[ens_lfo_freq1].val.f = log2(0.18);
    fxdata->p[ens_lfo_freq2].val.f = log2(5.52);

    fxdata->p[ens_lfo_depth1].val.f = 1.f;
    fxdata->p[ens_lfo_depth2].val.f = 0.6f;

    fxdata->p[ens_delay_type].val.i = 2;
    fxdata->p[ens_delay_clockrate].val.f = 40.f;
    fxdata->p[ens_delay_sat].val.f = 0.0f;
    fxdata->p[ens_delay_feedback].val.f = 0.f;

    fxdata->p[ens_output_filter].val.f = defaultFilterCut;
    fxdata->p[ens_output_filter].deactivated = false;
    fxdata->p[ens_width].val.f = 1.f;
    fxdata->p[ens_mix].val.f = 1.f;
}
} // namespace surge_mm::ensemble
