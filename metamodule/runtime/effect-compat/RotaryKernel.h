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
#include "rotary_support.h"
namespace surge_mm::rotary
{
using namespace std;
class RotaryKernel : public RotaryBase
{
  public:
    bool nativeRateBounds{true};
    lipol_ps_blocksz width alignas(16), mix alignas(16);
    sst::waveshapers::QuadWaveshaperState wsState alignas(16){};

    RotaryKernel(RotaryStorage *storage, RotarySettings *fxdata, RotaryValue *pd);
    ~RotaryKernel();
    void process_only_control();
    const char *get_effectname()
    {
        return "rotary";
    }
    void process(float *dataL, float *dataR);
    int get_ringout_decay()
    {
        return max_delay_length >> 5;
    }
    void setvars(bool init);
    void suspend();
    void init();
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);

    void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision);

    enum rotary_params
    {
        rot_horn_rate = 0,
        rot_doppler,
        rot_tremolo,
        rot_rotor_rate,
        rot_drive,
        rot_waveshape,
        rot_width,
        rot_mix,

        rot_num_params,
    };

  protected:
    float buffer[max_delay_length];
    int wpos;
    // filter *lp[2],*hp[2];
    // biquadunit rotor_lpL,rotor_lpR;
    BiquadFilter xover, lowbass;
    // float
    // f_rotor_lp[2][n_filter_parameters],f_xover[n_filter_parameters],f_lowbass[n_filter_parameters];

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc lfo;
    quadr_osc lf_lfo;
    lipol<float> dL, dR, hornamp[2];
    lag<float, true> drive;
    bool first_run;
};

inline RotaryKernel::RotaryKernel(RotaryStorage *storage, RotarySettings *fxdata, RotaryValue *pd)
    : RotaryBase(storage, fxdata, pd), xover(storage), lowbass(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
}

inline RotaryKernel::~RotaryKernel()
{
}

inline void RotaryKernel::init()
{
    memset(buffer, 0, max_delay_length * sizeof(float));

    wpos = 0;

    xover.suspend();
    lowbass.suspend();

    xover.coeff_LP2B(xover.calc_omega(0.862496f), 0.707); // 800 Hz
    lowbass.coeff_LP2B(xover.calc_omega(-1.14f), 0.707);  // 200 Hz

    setvars(true);
}

inline void RotaryKernel::setvars(bool init)
{
    drive.newValue(*pd_float[rot_drive]);
    width.set_target_smoothed(storage->db_to_linear(*pd_float[rot_width]));
    mix.set_target_smoothed(*pd_float[rot_mix]);

    if (init)
    {
        drive.instantize();
        width.instantize();
        mix.instantize();

        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
            wsState.R[i] = _mm_setzero_ps();
    }
}

inline void RotaryKernel::suspend()
{
    memset(buffer, 0, max_delay_length * sizeof(float));
    xover.suspend();
    lowbass.suspend();
    wpos = 0;
}

inline void RotaryKernel::init_default_values()
{
    fxdata->p[rot_horn_rate].val.f = 1.f;
    fxdata->p[rot_rotor_rate].val.f = 0.7;
    fxdata->p[rot_drive].val.f = 0.f;
    fxdata->p[rot_waveshape].val.i = 0;
    fxdata->p[rot_doppler].val.f = 0.25f;
    fxdata->p[rot_tremolo].val.f = 0.5f;
    fxdata->p[rot_width].val.f = 1.0f;
    fxdata->p[rot_mix].val.f = 1.0f;
}

inline void RotaryKernel::process_only_control()
{
    float frate = *pd_float[rot_horn_rate] *
                  (fxdata->p[rot_horn_rate].temposync ? storage->temposyncratio : 1.f);
    frate = nativeRateBounds ? std::clamp(frate, -32.f, 16.f) : std::min(frate, 127.f);

    lfo.set_rate(2 * M_PI * powf(2, frate) * storage->dsamplerate_inv * BLOCK_SIZE);
    lf_lfo.set_rate(*pd_float[rot_rotor_rate] * 2 * M_PI * powf(2, frate) *
                    storage->dsamplerate_inv * BLOCK_SIZE);

    lfo.process();
    lf_lfo.process();
}

inline void RotaryKernel::process(float *dataL, float *dataR)
{
    setvars(false);

    float frate = *pd_float[rot_horn_rate] *
                  (fxdata->p[rot_horn_rate].temposync ? storage->temposyncratio : 1.f);
    frate = nativeRateBounds ? std::clamp(frate, -32.f, 16.f) : std::min(frate, 127.f);

    /*
     ** lf_lfo.process drives the sub-frequency and processes inside the iteration over samples>
     ** lfo.process drives the speaker and processes outside the iteration
     ** therefore lf_lfo processes BLOCK_SIZE more times
     ** hence the lack of BLOCK_SIZE here
     */
    lfo.set_rate(2 * M_PI * powf(2, frate) * storage->dsamplerate_inv * BLOCK_SIZE);
    lf_lfo.set_rate(*pd_float[rot_rotor_rate] * 2 * M_PI * powf(2, frate) *
                    storage->dsamplerate_inv);

    float precalc0 = (-2 - (float)lfo.i);
    float precalc1 = (-1 - (float)lfo.r);
    float precalc2 = (+1 - (float)lfo.r);
    float lenL = sqrt(precalc0 * precalc0 + precalc1 * precalc1);
    float lenR = sqrt(precalc0 * precalc0 + precalc2 * precalc2);

    float delay = storage->samplerate * 0.0018f * *pd_float[rot_doppler];

    dL.newValue(delay * lenL);
    dR.newValue(delay * lenR);

    float dotp_L = (precalc1 * (float)lfo.r + precalc0 * (float)lfo.i) / lenL;
    float dotp_R = (precalc2 * (float)lfo.r + precalc0 * (float)lfo.i) / lenR;

    float a = *pd_float[rot_tremolo] * 0.6f;

    hornamp[0].newValue((1.f - a) + a * dotp_L);
    hornamp[1].newValue((1.f - a) + a * dotp_R);

    lfo.process();

    float upper alignas(16)[BLOCK_SIZE];
    float lower alignas(16)[BLOCK_SIZE];
    float lower_sub alignas(16)[BLOCK_SIZE];
    float tbufferL alignas(16)[BLOCK_SIZE];
    float tbufferR alignas(16)[BLOCK_SIZE];
    float wbL alignas(16)[BLOCK_SIZE];
    float wbR alignas(16)[BLOCK_SIZE];

    int k;

    drive.newValue(*pd_float[rot_drive]);

    int wsi = *pd_int[rot_waveshape];

    if (wsi < 0 || wsi >= n_fxws)
    {
        wsi = 0;
    }

    auto ws = FXWaveShapers[wsi];

    /*
    ** This is a set of completely empirical scaling settings to offset gain being too crazy
    ** in the drive cycle. There's no science really, just us playing with it and listening
    */
    float gain_tweak{1.f}, compensate{1.f}, drive_factor{1.f}, gain_comp_factor{1.0};
    float compensateStartsAt = 0.18;
    bool square_drive_comp = false;

    switch (ws)
    {
    case sst::waveshapers::WaveshaperType::wst_hard:
    {
        gain_tweak = 1.4;
        compensate = 3.f;
    }
    case sst::waveshapers::WaveshaperType::wst_asym:
    {
        gain_tweak = 1.15;
        compensate = 9.f;
        compensateStartsAt = 0.05;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_sine:
    {
        gain_tweak = 4.4;
        compensate = 10.f;
        compensateStartsAt = 0.f;
        square_drive_comp = true;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_digital:
    {
        gain_tweak = 1.f;
        compensate = 4.f;
        compensateStartsAt = 0.f;
        break;
    }
    case sst::waveshapers::WaveshaperType::wst_fwrectify:
    case sst::waveshapers::WaveshaperType::wst_fuzzsoft:
    {
        gain_tweak = 1.f;
        compensate = 2.f;
        compensateStartsAt = 0.f;
        break;
    }
    default:
    {
        gain_tweak = 1.f;
        compensate = 4.f;
        break;
    }
    }

    if (!fxdata->p[rot_drive].deactivated)
    {
        drive_factor = 1.f + (drive.v * drive.v * 15.f);

        if (drive.v < compensateStartsAt)
            gain_comp_factor = 1.0;
        else if (square_drive_comp)
            gain_comp_factor = 1.f + (((drive.v * drive.v) - compensateStartsAt) * compensate);
        else
            gain_comp_factor = 1.f + ((drive.v - compensateStartsAt) * compensate);
    }

    // FX waveshapers have value at wst_soft for 0; so don't add wst_soft here (like we did in 1.9)
    bool useSSEShaper = (ws >= sst::waveshapers::WaveshaperType::wst_sine);
    auto wsop = sst::waveshapers::GetQuadWaveshaper(ws);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        float input;

        if (!fxdata->p[rot_drive].deactivated)
        {
            drive_factor = 1.f + (drive.v * drive.v * 15.f);
            if (useSSEShaper)
            {
                auto inp = _mm_set1_ps(0.5 * (dataL[k] + dataR[k]));
                auto wsres = wsop(&wsState, inp, _mm_set1_ps(drive_factor));
                alignas(16) float r[4];
                _mm_store_ps(r, wsres);
                input = r[0] * gain_tweak;
            }
            else
            {
                input = storage->lookup_waveshape(ws, 0.5f * (dataL[k] + dataR[k]) * drive_factor) *
                        gain_tweak;
            }
            input /= gain_comp_factor;

            drive.process();
        }
        else
        {
            input = 0.5f * (dataL[k] + dataR[k]);
        }

        upper[k] = input;
        lower[k] = input;
    }

    xover.process_block(lower);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        // feed delay input
        int wp = (wpos + k) & (max_delay_length - 1);
        lower_sub[k] = lower[k];
        upper[k] -= lower[k];
        buffer[wp] = upper[k];

        int i_dtimeL = max(BLOCK_SIZE, min((int)dL.v, max_delay_length - FIRipol_N - 1));
        int i_dtimeR = max(BLOCK_SIZE, min((int)dR.v, max_delay_length - FIRipol_N - 1));

        int rpL = (wpos - i_dtimeL + k);
        int rpR = (wpos - i_dtimeR + k);

        int sincL = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtimeL + 1) - dL.v)), 0, FIRipol_M - 1);
        int sincR = FIRipol_N *
                    limit_range((int)(FIRipol_M * (float(i_dtimeR + 1) - dR.v)), 0, FIRipol_M - 1);

        // get delay output
        tbufferL[k] = 0;
        tbufferR[k] = 0;
        for (int i = 0; i < FIRipol_N; i++)
        {
            tbufferL[k] += buffer[(rpL - i) & (max_delay_length - 1)] *
                           storage->sinctable1X[sincL + FIRipol_N - i];
            tbufferR[k] += buffer[(rpR - i) & (max_delay_length - 1)] *
                           storage->sinctable1X[sincR + FIRipol_N - i];
        }
        dL.process();
        dR.process();
    }

    lowbass.process_block(lower_sub);

    for (k = 0; k < BLOCK_SIZE; k++)
    {
        lower[k] -= lower_sub[k];

        float bass = lower_sub[k] + lower[k] * (lf_lfo.r * 0.6f + 0.3f);

        wbL[k] = hornamp[0].v * tbufferL[k] + bass;
        wbR[k] = hornamp[1].v * tbufferR[k] + bass;

        lf_lfo.process();
        hornamp[0].process();
        hornamp[1].process();
    }

    // scale width
    applyWidth(wbL, wbR, width);

    mix.fade_2_blocks_inplace(dataL, wbL, dataR, wbR, BLOCK_SIZE_QUAD);

    wpos += BLOCK_SIZE;
    wpos = wpos & (max_delay_length - 1);
}
} // namespace surge_mm::rotary
