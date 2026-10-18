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
#include "chorus_support.h"
namespace surge_mm::chorus
{
using namespace std;
template <int v> class ChorusKernel : public ChorusBase
{
    lipol_ps_blocksz feedback alignas(16), mix alignas(16), width alignas(16);
    __m128 voicepanL4 alignas(16)[v], voicepanR4 alignas(16)[v];
    float buffer alignas(16)[max_delay_length + FIRipol_N]; // Includes padding so we can use SSE
                                                            // interpolation without wrapping

  public:
    void reset_native()
    {
        for (auto &t : time)
            t = {};
        lp.suspend();
        hp.suspend();
        feedback = {};
        mix = {};
        width = {};
        init();
    }
    enum chorus_params
    {
        ch_time,
        ch_rate,
        ch_depth,
        ch_feedback,
        ch_lowcut,
        ch_highcut,
        ch_mix,
        ch_width,

        ch_num_params,
    };

    ChorusKernel(ChorusStorage *storage, ChorusSettings *fxdata, ChorusValue *pd);
    ~ChorusKernel();
    const char *get_effectname()
    {
        return "chorus";
    }
    void init();
    void process(float *dataL, float *dataR);
    void suspend();
    void setvars(bool init);
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);

    void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision);

  private:
    lag<float, true> time[v];
    float voicepan[v][2];
    float envf;
    int wpos;
    BiquadFilter lp, hp;
    double lfophase[v];
};

template <int v>
ChorusKernel<v>::ChorusKernel(ChorusStorage *storage, ChorusSettings *fxdata, ChorusValue *pd)
    : ChorusBase(storage, fxdata, pd), lp(storage), hp(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    feedback.set_blocksize(BLOCK_SIZE);
}

template <int v> ChorusKernel<v>::~ChorusKernel()
{
}

template <int v> void ChorusKernel<v>::init()
{
    memset(buffer, 0, (max_delay_length + FIRipol_N) * sizeof(float));
    wpos = 0;
    envf = 0;
    const float gainscale = 1 / sqrt((float)v);

    for (int i = 0; i < v; i++)
    {
        time[i].setRate(0.001);
        float x = i;
        x /= (float)(v - 1);
        lfophase[i] = x;
        x = 2.f * x - 1.f;
        voicepan[i][0] = sqrt(0.5 - 0.5 * x) * gainscale;
        voicepan[i][1] = sqrt(0.5 + 0.5 * x) * gainscale;
        voicepanL4[i] = _mm_set1_ps(voicepan[i][0]);
        voicepanR4[i] = _mm_set1_ps(voicepan[i][1]);
    }

    setvars(true);
}

template <int v> void ChorusKernel<v>::setvars(bool init)
{
    if (init)
    {
        feedback.set_target(0.5f * amp_to_linear(fxdata->p[ch_feedback].val.f));
        hp.coeff_HP(hp.calc_omega(fxdata->p[ch_lowcut].val.f / 12.0), 0.707);
        lp.coeff_LP2B(lp.calc_omega(fxdata->p[ch_highcut].val.f / 12.0), 0.707);
        mix.set_target(fxdata->p[ch_mix].val.f);
        width.set_target(storage->db_to_linear(fxdata->p[ch_width].val.f));
    }
    else
    {
        feedback.set_target_smoothed(0.5f * amp_to_linear(*pd_float[ch_feedback]));
        float rate = storage->envelope_rate_linear(-*pd_float[1]) *
                     (fxdata->p[ch_rate].temposync ? storage->temposyncratio : 1.f);
        float tm = storage->note_to_pitch_ignoring_tuning(12 * *pd_float[0]) *
                   (fxdata->p[ch_time].temposync ? storage->temposyncratio_inv : 1.f);
        for (int i = 0; i < v; i++)
        {
            lfophase[i] += rate;

            if (lfophase[i] > 1)
                lfophase[i] = std::fmod(lfophase[i], 1.0);

            float lfoout = (2.f * fabs(2.f * lfophase[i] - 1.f) - 1.f) * *pd_float[ch_depth];
            time[i].newValue(storage->samplerate * tm * (1 + lfoout));
        }

        hp.coeff_HP(hp.calc_omega(*pd_float[ch_lowcut] * (1.f / 12.f)), 0.707);
        lp.coeff_LP2B(lp.calc_omega(*pd_float[ch_highcut] * (1.f / 12.f)), 0.707);
        mix.set_target_smoothed(*pd_float[ch_mix]);
        width.set_target_smoothed(storage->db_to_linear(*pd_float[ch_width]));
    }
}

template <int v> void ChorusKernel<v>::process(float *dataL, float *dataR)
{
    namespace mech = sst::basic_blocks::mechanics;
    namespace sdsp = sst::basic_blocks::dsp;

    setvars(false);

    float tbufferL alignas(16)[BLOCK_SIZE];
    float tbufferR alignas(16)[BLOCK_SIZE];
    float fbblock alignas(16)[BLOCK_SIZE];

    mech::clear_block<BLOCK_SIZE>(tbufferL);
    mech::clear_block<BLOCK_SIZE>(tbufferR);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        __m128 L = _mm_setzero_ps(), R = _mm_setzero_ps();

        for (int j = 0; j < v; j++)
        {
            time[j].process();
            float vtime = time[j].v;
            int i_dtime = max(BLOCK_SIZE, min((int)vtime, max_delay_length - FIRipol_N - 1));
            int rp = ((wpos - i_dtime + k) - FIRipol_N) & (max_delay_length - 1);
            int sinc = FIRipol_N * limit_range((int)(FIRipol_M * (float(i_dtime + 1) - vtime)), 0,
                                               FIRipol_M - 1);

            __m128 vo;
            vo = _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc]), _mm_loadu_ps(&buffer[rp]));
            vo = _mm_add_ps(vo, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc + 4]),
                                           _mm_loadu_ps(&buffer[rp + 4])));
            vo = _mm_add_ps(vo, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc + 8]),
                                           _mm_loadu_ps(&buffer[rp + 8])));

            L = _mm_add_ps(L, _mm_mul_ps(vo, voicepanL4[j]));
            R = _mm_add_ps(R, _mm_mul_ps(vo, voicepanR4[j]));
        }
        L = mech::sum_ps_to_ss(L);
        R = mech::sum_ps_to_ss(R);
        _mm_store_ss(&tbufferL[k], L);
        _mm_store_ss(&tbufferR[k], R);
    }

    if (!fxdata->p[ch_highcut].deactivated)
    {
        lp.process_block(tbufferL, tbufferR);
    }

    if (!fxdata->p[ch_lowcut].deactivated)
    {
        hp.process_block(tbufferL, tbufferR);
    }

    mech::add_block<BLOCK_SIZE>(tbufferL, tbufferR, fbblock);
    feedback.multiply_block(fbblock, BLOCK_SIZE_QUAD);
    sdsp::hardclip_block<BLOCK_SIZE>(fbblock);
    mech::accumulate_from_to<BLOCK_SIZE>(dataL, fbblock);
    mech::accumulate_from_to<BLOCK_SIZE>(dataR, fbblock);

    if (wpos + BLOCK_SIZE >= max_delay_length)
    {
        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            buffer[(wpos + k) & (max_delay_length - 1)] = fbblock[k];
        }
    }
    else
    {
        mech::copy_from_to<BLOCK_SIZE>(fbblock, &buffer[wpos]);
    }

    if (wpos < FIRipol_N)
        for (int k = 0; k < min(BLOCK_SIZE, FIRipol_N - wpos); k++)
            buffer[wpos + k + max_delay_length] = buffer[wpos + k];

    // scale width
    applyWidth(tbufferL, tbufferR, width);

    mix.fade_2_blocks_inplace(dataL, tbufferL, dataR, tbufferR, BLOCK_SIZE_QUAD);

    wpos += BLOCK_SIZE;
    wpos = wpos & (max_delay_length - 1);
}

template <int v> void ChorusKernel<v>::suspend()
{
    init();
}

template <int v> void ChorusKernel<v>::init_default_values()
{
    fxdata->p[ch_time].val.f = -6.f;
    fxdata->p[ch_rate].val.f = -2.f;
    fxdata->p[ch_depth].val.f = 0.3f;
    fxdata->p[ch_feedback].val.f = 0.5f;

    fxdata->p[ch_lowcut].val.f = -3.f * 12.f;
    fxdata->p[ch_lowcut].deactivated = false;

    fxdata->p[ch_highcut].val.f = 3.f * 12.f;
    fxdata->p[ch_highcut].deactivated = false;

    fxdata->p[ch_mix].val.f = 1.f;
    fxdata->p[ch_width].val.f = 0.f;
}
} // namespace surge_mm::chorus
