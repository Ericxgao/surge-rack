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
#include "resonator_support.h"
namespace surge_mm::resonator
{
class ResonatorKernel : public ResonatorBase
{
    lipol_ps_blocksz gain alignas(16);
    lipol_ps_blocksz mix alignas(16);

    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    void reset_native()
    {
        for (int c = 0; c < 2; ++c)
            qfus[c] = {};
        for (int e = 0; e < 3; ++e)
        {
            cutoff[e] = lag<float, true>{};
            resonance[e] = lag<float, true>{};
            bandGain[e] = lag<float, true>{};
            for (int c = 0; c < 2; ++c)
            {
                coeff[e][c].Reset();
                for (auto &r : Reg[e][c])
                    r = 0;
            }
        }
        init();
    }
    enum resonator_params
    {
        resonator_freq1 = 0,
        resonator_res1,
        resonator_gain1,

        resonator_freq2,
        resonator_res2,
        resonator_gain2,

        resonator_freq3,
        resonator_res3,
        resonator_gain3,

        resonator_mode,
        resonator_gain,
        resonator_mix,

        resonator_num_ctrls,
    };

    enum resonator_modes
    {
        rm_lowpass = 0,
        rm_bandpass,
        rm_bandpass_n,
        rm_highpass,

        rm_num_modes,
    };

    ResonatorKernel(ResonatorStorage *storage, ResonatorSettings *fxdata, ResonatorValue *pd);
    ~ResonatorKernel();
    const char *get_effectname()
    {
        return "Resonator";
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

    sst::filters::QuadFilterUnitState *qfus = nullptr;
    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    sst::filters::FilterCoefficientMaker<ResonatorStorage> coeff[3][2];
    lag<float, true> cutoff[3], resonance[3], bandGain[3];
    // float filterDelay[3][2][MAX_FB_COMB + FIRipol_N];
    // float WP[3][2];
    float Reg[3][2][sst::filters::n_filter_registers];

  private:
    int bi; // block increment (to keep track of events not occurring every n blocks)
};

inline ResonatorKernel::ResonatorKernel(ResonatorStorage *storage, ResonatorSettings *fxdata,
                                        ResonatorValue *pd)
    : ResonatorBase(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true)
{
    gain.set_blocksize(BLOCK_SIZE);
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
        }
    }
}

inline ResonatorKernel::~ResonatorKernel()
{
    delete[] qfus;
}

inline void ResonatorKernel::init()
{
    sampleRateReset();
    setvars(true);
    bi = 0;
}

inline void ResonatorKernel::setvars(bool init)
{
    if (init)
    {

        for (int i = 0; i < 3; ++i)
        {
            cutoff[i].instantize();
            resonance[i].instantize();
            bandGain[i].instantize();
        }

        gain.set_target(1.f);
        mix.set_target(1.f);

        gain.instantize();
        mix.instantize();

        halfbandOUT.reset();
        halfbandIN.reset();
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            cutoff[i].newValue(fxdata->p[resonator_freq1 + i * 3].get_extended(
                *pd_float[resonator_freq1 + i * 3]));
            resonance[i].newValue(
                fxdata->p[resonator_res1 + i * 3].get_extended(*pd_float[resonator_res1 + i * 3]));
            bandGain[i].newValue(amp_to_linear(*pd_float[resonator_gain1 + i * 3]));
        }
    }
}

inline void ResonatorKernel::sampleRateReset()
{
    for (int e = 0; e < 3; ++e)
        for (int c = 0; c < 2; ++c)
            coeff[e][c].setSampleRateAndBlockSize((float)storage->dsamplerate_os, BLOCK_SIZE_OS);
}

inline void ResonatorKernel::process(float *dataL, float *dataR)
{
    if (bi == 0)
    {
        setvars(false);
    }
    bi = (bi + 1) & slowrate_m1;

    float dataOS alignas(16)[2][BLOCK_SIZE_OS];

    // Upsample the input
    halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1], BLOCK_SIZE_OS);

    /*
     * Select the coefficients. Here you have to base yourself on the mode switch and
     * call CoeffMaker with one fo the known models and subtypes, along with the resonance
     * and the frequency of the particular band.
     */
    using namespace sst::filters;
    auto whichModel = *pd_int[resonator_mode];
    FilterType type;
    FilterSubType subtype = st_Driven;

    switch (whichModel)
    {
    case rm_lowpass:
    {
        type = fut_lp12;
        break;
    }
    case rm_bandpass:
    case rm_bandpass_n:
    {
        type = fut_bp12;
        break;
    }
    case rm_highpass:
    {
        type = fut_hp12;
        break;
    }
    default:
    {
        type = fut_none;
        break;
    }
    }

    auto filtptr = GetQFPtrFilterUnit(type, subtype);
    float rescomp[rm_num_modes] = {0.75, 0.9, 0.9, 0.75}; // prevent self-oscillation

    for (int i = 0; i < 3; ++i)
    {
        auto boundcutoff = limit_range(
            fxdata->p[resonator_freq1 + i * 3].get_extended(*pd_float[resonator_freq1 + i * 3]),
            fxdata->p[resonator_freq1 + i * 3].val_min.f,
            fxdata->p[resonator_freq1 + i * 3].val_max.f);

        float resval;

        if (fxdata->p[resonator_res1 + i * 3].extend_range)
        {
            resval = *pd_float[resonator_res1 + i * 3];
        }
        else
        {
            resval = limit_range(*pd_float[resonator_res1 + i * 3],
                                 fxdata->p[resonator_res1 + i * 3].val_min.f,
                                 fxdata->p[resonator_res1 + i * 3].val_max.f);
        }

        cutoff[i].newValue(boundcutoff);
        resonance[i].newValue(resval);
        bandGain[i].newValue(amp_to_linear(*pd_float[resonator_gain1 + i * 3]));
    }

    /*
     * So now set up across the voices (e for 'entry' to match SurgeVoice) and the channels (c)
     */
    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            coeff[e][c].MakeCoeffs(cutoff[e].v, resonance[e].v * rescomp[whichModel], type, subtype,
                                   storage, false);

            coeff[e][c].updateState(qfus[c], e);

            for (int i = 0; i < n_filter_registers; i++)
            {
                set1f(qfus[c].R[i], e, Reg[e][c][i]);
            }

            qfus[c].active[e] = 0xFFFFFFFF;
            qfus[c].active[3] = 0;
        }
    }

    /* Run the filters */
    for (int s = 0; s < BLOCK_SIZE_OS; ++s)
    {
        // preprocess audio in through asymmetric waveshaper
        // this mimics Polymoog's power supply which only operated on positive rails
        dataOS[0][s] =
            storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_asym, dataOS[0][s]);
        dataOS[1][s] =
            storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_asym, dataOS[1][s]);

        auto l128 = _mm_setzero_ps();
        auto r128 = _mm_setzero_ps();

        if (filtptr)
        {
            l128 = filtptr(&(qfus[0]), _mm_set1_ps(dataOS[0][s]));
            r128 = filtptr(&(qfus[1]), _mm_set1_ps(dataOS[1][s]));
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
            if (whichModel == 2 && i == 1)
            {
                mixl -= tl[i] * bandGain[i].v;
                mixr -= tr[i] * bandGain[i].v;
            }
            else
            {
                mixl += tl[i] * bandGain[i].v;
                mixr += tr[i] * bandGain[i].v;
            }

            // soft-clip output for good measure
            mixl = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixl);
            mixr = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixr);

            // lag class only works at BLOCK_SIZE time, not BLOCK_SIZE_OS, so call process every
            // other sample
            if (s % 2 == 0)
            {
                cutoff[i].process();
                resonance[i].process();
                bandGain[i].process();
            }
        }

        dataOS[0][s] = mixl;
        dataOS[1][s] = mixr;
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

    /* Downsample out */
    halfbandOUT.process_block_D2(dataOS[0], dataOS[1], BLOCK_SIZE_OS);
    namespace mech = sst::basic_blocks::mechanics;
    mech::copy_from_to<BLOCK_SIZE>(dataOS[0], L);
    mech::copy_from_to<BLOCK_SIZE>(dataOS[1], R);

    gain.set_target_smoothed(db_to_linear(*pd_float[resonator_gain]));
    gain.multiply_2_blocks(L, R, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(clamp1bp(*pd_float[resonator_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

inline void ResonatorKernel::suspend()
{
    init();
}
} // namespace surge_mm::resonator
