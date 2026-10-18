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
#include "vocoder_support.h"
namespace surge_mm::vocoder
{
namespace mech = sst::basic_blocks::mechanics;
inline constexpr int n_vocoder_bands = 20, voc_vector_size = 5;
class VocoderKernelFilter
{
  public:
    VocoderKernelFilter();

    void Reset();

    void SetCoeff(float Omega[4], float Q, float Spread);

    void CopyCoeff(const VocoderKernelFilter &SVF);

    inline vFloat CalcBPF(vFloat In)
    {
        L1 = vMAdd(F1, B1, L1);
        vFloat H1 = vNMSub(Q, B1, vSub(vMul(In, Q), L1));
        B1 = vMAdd(F1, H1, B1);

        L2 = vMAdd(F2, B2, L2);
        vFloat H2 = vNMSub(Q, B2, vSub(vMul(B1, Q), L2));
        B2 = vMAdd(F2, H2, B2);

        return B2;
    }

  private:
    float CalcF(float Omega);
    float CalcQ(float Quality);

    // Registers
    vFloat L1, B1, L2, B2;

    // Coefficients
    vFloat F1, F2, Q;
};

inline VocoderKernelFilter::VocoderKernelFilter()
{
    Reset();
}

inline void VocoderKernelFilter::Reset()
{
    L1 = vZero;
    L2 = vZero;
    B1 = vZero;
    B2 = vZero;
    F1 = vZero;
    F2 = vZero;
    Q = vZero;
}

inline void VocoderKernelFilter::SetCoeff(float pOmega[4], float QVal, float Spread)
{
    float Freq1 alignas(16)[4];
    float Freq2 alignas(16)[4];
    float Quality alignas(16)[4];

    QVal = CalcQ(QVal);

    for (int i = 0; i < 4; i++)
    {
        Freq1[i] = CalcF(pOmega[i] * (1.f - Spread));
        Freq2[i] = CalcF(pOmega[i] * (1.f + Spread));
        Quality[i] = QVal;
    }

    F1 = vLoad(Freq1);
    F2 = vLoad(Freq2);
    Q = vLoad(Quality);
}

inline void VocoderKernelFilter::CopyCoeff(const VocoderKernelFilter &SVF)
{
    F1 = SVF.F1;
    F2 = SVF.F2;
    Q = SVF.Q;
}

inline float VocoderKernelFilter::CalcF(float Omega)
{
    return 2.0 * sin(M_PI * Omega);
}

inline float VocoderKernelFilter::CalcQ(float Quality)
{
    return 1.f / Quality;
}
class VocoderKernel : public VocoderBase
{
  public:
    void reset_native()
    {
        for (int i = 0; i < voc_vector_size; ++i)
        {
            mCarrierL[i].Reset();
            mCarrierR[i].Reset();
            mModulator[i].Reset();
            mModulatorR[i].Reset();
            mEnvF[i] = vZero;
            mEnvFR[i] = vZero;
        }
        mGain.set_target(0);
        mGain.instantize();
        mGainR.set_target(0);
        mGainR.instantize();
        mBI = 0;
        init();
    }
    enum vocoder_input_modes
    {
        vim_mono,
        vim_left,
        vim_right,
        vim_stereo,
    };

    enum vocoder_params
    {
        voc_input_gain,
        voc_input_gate,

        voc_envfollow,
        voc_q,
        voc_shift,

        voc_num_bands,
        voc_minfreq,
        voc_maxfreq,

        voc_mod_input,
        voc_mod_range,
        voc_mod_center,

        voc_mix,

        // voc_unvoiced_threshold,

        voc_num_params,
    };

    VocoderKernel(VocoderStorage *storage, VocoderSettings *fxdata, VocoderValue *pd);
    ~VocoderKernel();
    const char *get_effectname()
    {
        return "vocoder";
    }
    void init();
    void process(float *dataL, float *dataR);
    void suspend();
    int get_ringout_decay()
    {
        return 500;
    }
    void setvars(bool init);
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);

    void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision);

  private:
    VocoderKernelFilter mCarrierL alignas(16)[voc_vector_size];
    VocoderKernelFilter mCarrierR alignas(16)[voc_vector_size];
    VocoderKernelFilter mModulator alignas(16)[voc_vector_size];
    VocoderKernelFilter mModulatorR alignas(16)[voc_vector_size];
    vFloat mEnvF alignas(16)[voc_vector_size];
    vFloat mEnvFR alignas(16)[voc_vector_size];
    lipol_ps_blocksz mGain alignas(16);
    lipol_ps_blocksz mGainR alignas(16);
    int modulator_mode;
    float wet;
    int mBI; // block increment (to keep track of events not occurring every n blocks)
    int active_bands;

    /*
    float mVoicedLevel;
    float mUnvoicedLevel;

    BiquadFilter
            mVoicedDetect,
            mUnvoicedDetect;
    */
};

inline VocoderKernel::VocoderKernel(VocoderStorage *storage, VocoderSettings *fxdata,
                                    VocoderValue *pd)
    : VocoderBase(storage, fxdata, pd), mBI(0)
{
    /*mVoicedDetect.storage = storage;
    mUnvoicedDetect.storage = storage;
    mVoicedLevel = 0.f;
    mUnvoicedLevel = 0.f;*/

    active_bands = n_vocoder_bands;
    mGain.set_blocksize(BLOCK_SIZE);
    mGainR.set_blocksize(BLOCK_SIZE);
    for (int i = 0; i < voc_vector_size; i++)
    {
        mEnvF[i] = vZero;
        mEnvFR[i] = vZero;
    }
}

inline VocoderKernel::~VocoderKernel()
{
}

inline void VocoderKernel::init()
{
    setvars(true);
}

inline void VocoderKernel::setvars(bool init)
{
    modulator_mode = *pd_int[voc_mod_input];
    wet = *pd_float[voc_mix];
    float Freq[4], FreqM[4];

    const float Q = 20.f * (1.f + 0.5f * *pd_float[voc_q]);
    const float Spread = 0.4f / Q;

    active_bands = *pd_int[voc_num_bands];
    // FIXME - adjust the UI to be chunks of 4
    active_bands = active_bands - (active_bands % 4);

    // We need to clamp these in reasonable ranges
    float flo = limit_range(*pd_float[voc_minfreq], -36.f, 36.f);
    float fhi = limit_range(*pd_float[voc_maxfreq], 0.f, 60.f);

    if (flo > fhi)
    {
        auto t = fhi;
        fhi = flo;
        flo = t;
    }
    float df = (fhi - flo) / (active_bands - 1);

    float hzlo = 440.f * pow(2.f, flo / 12.f);
    float dhz = pow(2.f, df / 12.f);

    float fb = hzlo;

    float mb = fb;
    float mdhz = dhz;
    bool sepMod = false;

    float mC = *pd_float[voc_mod_center];
    float mX = *pd_float[voc_mod_range];

    if (mC != 0 || mX != 0)
    {
        sepMod = true;
        auto fDist = fhi - flo;
        auto fDistHalf = fDist / 2.f;

        // that 0.3 is a tuning choice about how far we can move center
        auto mMid = fDistHalf + flo + 0.3 * mC * fDistHalf;
        // as is that 0.7
        auto mLo = mMid - fDistHalf * (1 + 0.7 * mX);
        auto dM = fDistHalf * 2 * (1.0 + 0.7 * mX) / (active_bands - 1);

        auto mHi = mLo + dM * (active_bands - 1);

        if (mHi > 60)
            dM = (60 - mLo) / (active_bands - 1);

        mHi = mLo + dM * (active_bands - 1);

        mb = 440.0 * pow(2.f, mLo / 12.f);
        mdhz = pow(2.f, dM / 12.f);
    }

    for (int i = 0; i < active_bands && i < n_vocoder_bands; i++)
    {
        Freq[i & 3] = fb * storage->samplerate_inv;
        FreqM[i & 3] = mb * storage->samplerate_inv;

        if ((i & 3) == 3)
        {
            int j = i >> 2;
            mCarrierL[j].SetCoeff(Freq, Q, Spread);
            mCarrierR[j].CopyCoeff(mCarrierL[j]);
            if (sepMod)
            {
                mModulator[j].SetCoeff(FreqM, Q, Spread);
                if (modulator_mode == vim_stereo)
                {
                    mModulatorR[j].SetCoeff(FreqM, Q, Spread);
                }
                else
                {
                    mModulatorR[j].CopyCoeff(mModulator[j]);
                }
            }
            else
            {
                mModulator[j].CopyCoeff(mCarrierL[j]);
                mModulatorR[j].CopyCoeff(mCarrierR[j]);
            }
        }
        fb *= dhz;
        mb *= mdhz;
    }

    /*mVoicedDetect.coeff_LP(BiquadFilter::calc_omega_from_Hz(1000.f), 0.707);
    mUnvoicedDetect.coeff_HP(BiquadFilter::calc_omega_from_Hz(5000.f), 0.707);

    if (init)
    {
       mVoicedDetect.coeff_instantize();
       mUnvoicedDetect.coeff_instantize();
    }*/
}

inline void VocoderKernel::process(float *dataL, float *dataR)
{
    mBI = (mBI + 1) & 0x3f;

    if (mBI == 0)
    {
        setvars(false);
    }
    modulator_mode = *(pd_int[voc_mod_input]); // fxdata->p[voc_mod_input].val.i;
    wet = *pd_float[voc_mix];
    float EnvFRate = 0.001f * powf(2.f, 4.f * *pd_float[voc_envfollow]);

    // the left channel variables are used for mono when stereo is disabled
    float modulator_in alignas(16)[BLOCK_SIZE];
    float modulator_inR alignas(16)[BLOCK_SIZE]{};

    if (modulator_mode == vim_mono)
    {
        mech::add_block<BLOCK_SIZE>(storage->audio_in_nonOS[0], storage->audio_in_nonOS[1],
                                    modulator_in);
    }
    else
    {
        mech::copy_from_to<BLOCK_SIZE>(storage->audio_in_nonOS[0], modulator_in);
        mech::copy_from_to<BLOCK_SIZE>(storage->audio_in_nonOS[1], modulator_inR);
    }

    float Gain = *pd_float[voc_input_gain] + 24.f;
    mGain.set_target_smoothed(storage->db_to_linear(Gain));
    mGain.multiply_block(modulator_in, BLOCK_SIZE_QUAD);

    mGainR.set_target_smoothed(storage->db_to_linear(Gain));
    mGainR.multiply_block(modulator_inR, BLOCK_SIZE_QUAD);

    vFloat Rate = vLoad1(EnvFRate);
    vFloat Ratem1 = vLoad1(1.f - EnvFRate);

    float Gate = storage->db_to_linear(*pd_float[voc_input_gate] + Gain);
    vFloat GateLevel = vLoad1(Gate * Gate);

    const vFloat MaxLevel = vLoad1(6.f);

    // Voiced / Unvoiced detection
    /*   mVoicedDetect.process_block_to(modulator_in, modulator_tbuf);
       float a = min(4.f, get_squaremax(modulator_tbuf,BLOCK_SIZE_QUAD));
       mVoicedLevel = mVoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

       mUnvoicedDetect.process_block_to(modulator_in, modulator_tbuf);
       a = min(4.f, get_squaremax(modulator_tbuf, BLOCK_SIZE_QUAD));
       mUnvoicedLevel = mUnvoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

       float Ratio = db_to_linear(*pd_float[voc_unvoiced_threshold]);

       if (mUnvoicedLevel > (mVoicedLevel * Ratio))
         // mix carrier with noise
         for(int i = 0; i < BLOCK_SIZE; i++)
         {
            float rand11 = storage->rand_pm1();
            dataL[i] = rand11;
            rand11 = storage->rand_pm1();
            dataR[i] = rand11;
         }*/

    if (modulator_mode == vim_mono || modulator_mode == vim_left || modulator_mode == vim_right)
    {
        float *input;

        if (modulator_mode == vim_mono || modulator_mode == vim_left)
        {
            input = modulator_in;
        }
        else
        {
            input = modulator_inR;
        }

        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            vFloat In = vLoad1(input[k]);

            vFloat Left = vLoad1(dataL[k]);
            vFloat Right = vLoad1(dataR[k]);

            vFloat LeftSum = vZero;
            vFloat RightSum = vZero;

            for (int j = 0; (j < (active_bands >> 2)) && (j < voc_vector_size); j++)
            {
                vFloat Mod = mModulator[j].CalcBPF(In);
                Mod = vMin(vMul(Mod, Mod), MaxLevel);
                Mod = vAnd(Mod, vCmpGE(Mod, GateLevel));
                mEnvF[j] = vMAdd(mEnvF[j], Ratem1, vMul(Rate, Mod));
                Mod = vSqrtFast(mEnvF[j]);

                LeftSum = vAdd(LeftSum, mCarrierL[j].CalcBPF(vMul(Left, Mod)));
                RightSum = vAdd(RightSum, mCarrierR[j].CalcBPF(vMul(Right, Mod)));
            }

            float inMul = 1.0 - wet;
            dataL[k] = dataL[k] * inMul + wet * vSum(LeftSum) * 4.f;
            dataR[k] = dataR[k] * inMul + wet * vSum(RightSum) * 4.f;
        }
    }
    else if (modulator_mode == vim_stereo)
    {
        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            vFloat InL = vLoad1(modulator_in[k]);
            vFloat InR = vLoad1(modulator_inR[k]);
            vFloat Left = vLoad1(dataL[k]);
            vFloat Right = vLoad1(dataR[k]);

            vFloat LeftSum = vZero;
            vFloat RightSum = vZero;

            for (int j = 0; j < (active_bands >> 2) && j < (n_vocoder_bands >> 2); j++)
            {
                vFloat ModL = mModulator[j].CalcBPF(InL);
                vFloat ModR = mModulatorR[j].CalcBPF(InR);
                ModL = vMin(vMul(ModL, ModL), MaxLevel);
                ModR = vMin(vMul(ModR, ModR), MaxLevel);

                ModL = vAnd(ModL, vCmpGE(ModL, GateLevel));
                ModR = vAnd(ModR, vCmpGE(ModR, GateLevel));

                mEnvF[j] = vMAdd(mEnvF[j], Ratem1, vMul(Rate, ModL));
                mEnvFR[j] = vMAdd(mEnvFR[j], Ratem1, vMul(Rate, ModR));
                ModL = vSqrtFast(mEnvF[j]);
                ModR = vSqrtFast(mEnvFR[j]);
                LeftSum = vAdd(LeftSum, mCarrierL[j].CalcBPF(vMul(Left, ModL)));
                RightSum = vAdd(RightSum, mCarrierR[j].CalcBPF(vMul(Right, ModR)));
            }

            float inMul = 1.0 - wet;
            dataL[k] = dataL[k] * inMul + wet * vSum(LeftSum) * 4.f;
            dataR[k] = dataR[k] * inMul + wet * vSum(RightSum) * 4.f;
        }
    }
}

inline void VocoderKernel::suspend()
{
    init();
}
} // namespace surge_mm::vocoder
