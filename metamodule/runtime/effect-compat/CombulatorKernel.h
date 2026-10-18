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
#include "combulator_support.h"
namespace surge_mm::combulator
{
class CombulatorKernel : public CombulatorBase
{
    lipol_ps_blocksz input alignas(16), mix alignas(16), negone alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    bool rack_history{};
    void reset_native()
    {
        for (int c = 0; c < 2; ++c)
            qfus[c] = {};
        for (int e = 0; e < 3; ++e)
        {
            freq[e] = {};
            gain[e] = {};
            for (int c = 0; c < 2; ++c)
            {
                coeff[e][c].Reset();
                WP[e][c] = 0;
                for (auto &r : Reg[e][c])
                    r = 0;
            }
        }
        feedback = {};
        pan2 = {};
        pan3 = {};
        tone = {};
        noisemix = {};
        lp.suspend();
        hp.suspend();
        storage->reset_random();
        init();
    }
    enum combulator_params
    {
        combulator_noise_mix = 0,

        combulator_freq1,
        combulator_freq2,
        combulator_freq3,
        combulator_feedback,
        combulator_tone,

        combulator_gain1,
        combulator_gain2,
        combulator_gain3,

        combulator_pan2,
        combulator_pan3,

        combulator_mix,

        combulator_num_ctrls,
    };

    CombulatorKernel(CombulatorStorage *storage, CombulatorSettings *fxdata, CombulatorValue *pd);
    ~CombulatorKernel();
    const char *get_effectname()
    {
        return "Combulator";
    }
    void init();
    void sampleRateReset();
    void process(float *dataL, float *dataR);
    int get_ringout_decay()
    {
        return -1;
    }
    void suspend();
    void setvars(bool init);
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);
    void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision);

    sst::filters::QuadFilterUnitState *qfus = nullptr;
    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    sst::filters::FilterCoefficientMaker<CombulatorStorage> coeff[3][2];
    BiquadFilter lp, hp;
    lag<float, true> freq[3], feedback, gain[3], pan2, pan3, tone, noisemix;
    float filterDelay[3][2][MAX_FB_COMB_EXTENDED + FIRipol_N];
    float WP[3][2];
    float Reg[3][2][sst::filters::n_filter_registers];

    static constexpr int PANLAW_SIZE = 4096; // power of 2 please
    float panL[PANLAW_SIZE], panR[PANLAW_SIZE];

    float envA, envR, envV[2];
    float noiseGen[2][2];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
};

inline CombulatorKernel::CombulatorKernel(CombulatorStorage *storage, CombulatorSettings *fxdata,
                                          CombulatorValue *pd)
    : CombulatorBase(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true), lp(storage),
      hp(storage)
{
    lp.setBlockSize(BLOCK_SIZE);
    hp.setBlockSize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);

    qfus = new sst::filters::QuadFilterUnitState[2]();

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int q = 0; q < sst::filters::n_filter_registers; ++q)
            {
                Reg[e][c][q] = 0;
            }
            WP[e][c] = 0;
        }
    }

    memset(filterDelay, 0, 3 * 2 * (MAX_FB_COMB_EXTENDED + FIRipol_N) * sizeof(float));

    // http://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
    for (int i = 0; i < PANLAW_SIZE; ++i)
    {
        auto piby2 = M_PI / 2.0;
        double panAngle = 1.0 * i / (PANLAW_SIZE - 1) * piby2;

        panL[i] = sqrt((piby2 - panAngle) / piby2 * cos(panAngle));
        panR[i] = sqrt(panAngle * sin(panAngle) / piby2);
    }

    // A very simple envelope follower
    envA = pow(0.01, 1.0 / (5 * storage->dsamplerate_os * 0.001));
    envR = pow(0.01, 1.0 / (5 * storage->dsamplerate_os * 0.001));
    envV[0] = 0.f;
    envV[1] = 0.f;

    noiseGen[0][0] = 0.f;
    noiseGen[1][0] = 0.f;
    noiseGen[0][1] = 0.f;
    noiseGen[1][1] = 0.f;
}

inline CombulatorKernel::~CombulatorKernel()
{
    delete[] qfus;
}

inline void CombulatorKernel::init()
{
    sampleRateReset();
    setvars(true);

    bi = 0;
    lp.suspend();

    memset(filterDelay, 0, 3 * 2 * (MAX_FB_COMB_EXTENDED + FIRipol_N) * sizeof(float));

    envV[0] = 0.f;
    envV[1] = 0.f;

    noiseGen[0][0] = 0.f;
    noiseGen[1][0] = 0.f;
    noiseGen[0][1] = 0.f;
    noiseGen[1][1] = 0.f;
}

inline void CombulatorKernel::setvars(bool init)
{
    for (int i = 0; i < 3; ++i)
    {
        if (i == 0)
        {
            freq[i].newValue(*pd_float[combulator_freq1]);
        }
        else
        {
            if (fxdata->p[combulator_freq1 + i].extend_range)
            {
                freq[i].newValue(*pd_float[combulator_freq1 + i]);
            }
            else
            {
                freq[i].newValue(*pd_float[combulator_freq1] + *pd_float[combulator_freq1 + i]);
            }
        }

        gain[i].newValue(amp_to_linear(limit_range(*pd_float[combulator_gain1 + i], 0.f, 2.f)));
    }

    noisemix.newValue(clamp01(*pd_float[combulator_noise_mix]));
    feedback.newValue(*pd_float[combulator_feedback]);
    tone.newValue(clamp1bp(*pd_float[combulator_tone]));
    pan2.newValue(clamp1bp(*pd_float[combulator_pan2]));
    pan3.newValue(clamp1bp(*pd_float[combulator_pan3]));

    negone.set_target(-1.f);

    if (init)
    {
        for (int i = 0; i < 3; ++i)
        {
            freq[i].instantize();
            gain[i].instantize();
        }

        feedback.instantize();

        pan2.instantize();
        pan3.instantize();
        tone.instantize();
        noisemix.instantize();

        mix.set_target(1.f);
        mix.instantize();

        negone.set_target(-1.f);
        negone.instantize();

        lp.coeff_instantize();
        hp.coeff_instantize();

        halfbandOUT.reset();
        halfbandIN.reset();
    }
    else
    {
        // lowpass range is from MIDI note 136 down to 57 (~21.1 kHz down to 220 Hz)
        // highpass range is from MIDI note 34 to 136(~61 Hz to ~21.1 kHz)
        float clo = -12, cmid = 67, chi = -33;
        float hpCutoff = chi;
        float lpCutoff = cmid;

        if (tone.v > 0)
        {
            // OK so cool scale the hp cutoff
            auto tv = tone.v;
            hpCutoff = tv * (cmid - chi) + chi;
        }
        else
        {
            auto tv = -tone.v;
            lpCutoff = tv * (clo - cmid) + cmid;
        }

        lp.coeff_LP(lp.calc_omega((lpCutoff / 12.0) - 2.f), 0.707);
        hp.coeff_HP(hp.calc_omega((hpCutoff / 12.0) - 2.f), 0.707);
    }
}

inline void CombulatorKernel::sampleRateReset()
{
    if (!rack_history)
        envA = envR = pow(0.01, 1.0 / (5 * storage->dsamplerate_os * 0.001));
    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            coeff[e][c].setSampleRateAndBlockSize((float)storage->dsamplerate_os, BLOCK_SIZE_OS);
        }
    }
}

inline void CombulatorKernel::process(float *dataL, float *dataR)
{
    setvars(false);

    // Upsample the input
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];

    halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1], BLOCK_SIZE_OS);

    /*
     * Select the coefficients. Here you have to base yourself on the mode switch and
     * call CoeffMaker with one fo the known models and subtypes, along with the feedback
     * and the frequency of the particular band.
     */
    using namespace sst::filters;

    int type = fut_comb_pos, subtype = 1;

    auto filtptr =
        GetQFPtrFilterUnit(static_cast<FilterType>(type),
                           static_cast<FilterSubType>(subtype | QFUSubtypeMasks::EXTENDED_COMB));

    auto fbscaled = (feedback.v < 0.f ? -1.f : 1.f) * sqrt(abs(feedback.v));

    /*
     * So now set up across the voices (e for 'entry' to match SurgeVoice) and the channels (c)
     */
    bool useTuning = fxdata->p[combulator_freq1].extend_range;

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            coeff[e][c].MakeCoeffs(
                freq[e].v, fbscaled, static_cast<FilterType>(type),
                static_cast<FilterSubType>(subtype | QFUSubtypeMasks::EXTENDED_COMB), storage,
                useTuning);

            coeff[e][c].updateState(qfus[c], e);

            for (int i = 0; i < n_filter_registers; i++)
            {
                set1f(qfus[c].R[i], e, Reg[e][c][i]);
            }

            qfus[c].DB[e] = filterDelay[e][c];
            qfus[c].WP[e] = WP[e][c];

            qfus[c].active[e] = 0xFFFFFFFF;
            qfus[c].active[3] = 0;
        }
    }

    /* Run the filters */
    float noise[2];

    for (int s = 0; s < BLOCK_SIZE_OS; ++s)
    {
        // Envelope Follower Update
        for (int c = 0; c < 2; ++c)
        {
            auto v = dataOS[c][s];
            auto e = envV[c];

            if (v > e)
            {
                e = envA * (e - v) + v;
            }
            else
            {
                e = envR * (e - v) + v;
            }

            envV[c] = e;
            noise[c] = noisemix.v * 3.f * envV[c] *
                       sst::basic_blocks::dsp::correlated_noise_o2mk2_supplied_value(
                           noiseGen[c][0], noiseGen[c][1], 0, storage->rand_pm1());
        }

        auto l128 = _mm_setzero_ps();
        auto r128 = _mm_setzero_ps();

        // FIXME - we want to interpolate the non-integral part if we like this
        int panIndex2 = (int)((limit_range(pan2.v, -1.f, 1.f) + 1) * ((PANLAW_SIZE - 1) / 2)) &
                        (PANLAW_SIZE - 1);
        int panIndex3 = (int)((limit_range(pan3.v, -1.f, 1.f) + 1) * ((PANLAW_SIZE - 1) / 2)) &
                        (PANLAW_SIZE - 1);

        if (filtptr)
        {
            l128 = filtptr(&(qfus[0]), _mm_set1_ps(dataOS[0][s] + noise[0]));
            r128 = filtptr(&(qfus[1]), _mm_set1_ps(dataOS[1][s] + noise[1]));
        }
        else
        {
            l128 = _mm_set1_ps(dataOS[0][s]);
            r128 = _mm_set1_ps(dataOS[1][s]);
        }

        float mixl = 0, mixr = 0;
        alignas(16) float tl[4], tr[4];

        _mm_store_ps(tl, l128);
        _mm_store_ps(tr, r128);

        for (int i = 0; i < 3; ++i)
        {
            float panl = 0.59, panr = 0.59;

            if (i == 1)
            {
                panl = panL[panIndex2];
                panr = panR[panIndex2];
            }
            else if (i == 2)
            {
                // The other way!
                panl = panL[panIndex3];
                panr = panR[panIndex3];
            }

            mixl += tl[i] * gain[i].v * panl / 0.59;
            mixr += tr[i] * gain[i].v * panr / 0.59;
        }

        // soft-clip output for good measure
        mixl = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixl);
        mixr = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixr);

        dataOS[0][s] = mixl;
        dataOS[1][s] = mixr;

        // lag class only works at BLOCK_SIZE time, not BLOCK_SIZE_OS,
        // so call process every other sample
        if (s % 2 == 0)
        {
            for (auto i = 0; i < 3; ++i)
            {
                freq[i].process();
                gain[i].process();

                if (i == 0)
                {
                    feedback.process();
                    pan2.process();
                    pan3.process();
                    tone.process();
                    noisemix.process();
                }
            }
        }
    }

    /* preserve those registers and stuff */
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < n_cm_coeffs; i++)
        {
            for (int e = 0; e < 3; ++e)
            {
                coeff[e][c].C[i] = get1f(qfus[c].C[i], e);
            }
        }

        for (int i = 0; i < n_filter_registers; i++)
        {
            for (int e = 0; e < 3; ++e)
            {
                Reg[e][c][i] = get1f(qfus[c].R[i], e);
            }
        }
    }

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            WP[e][c] = qfus[c].WP[e];
        }
    }

    /* Downsample out */
    halfbandOUT.process_block_D2(dataOS[0], dataOS[1], BLOCK_SIZE_OS);

    sst::basic_blocks::mechanics::copy_from_to<BLOCK_SIZE>(dataOS[0], L);
    sst::basic_blocks::mechanics::copy_from_to<BLOCK_SIZE>(dataOS[1], R);

    if (!fxdata->p[combulator_tone].deactivated)
    {
        lp.process_block(L, R);
        hp.process_block(L, R);
    }

    auto cm = clamp01(*pd_float[combulator_mix]);

    mix.set_target_smoothed(cm);
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

inline void CombulatorKernel::suspend()
{
    init();
}
} // namespace surge_mm::combulator
