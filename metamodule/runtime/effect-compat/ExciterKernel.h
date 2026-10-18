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
#include "exciter_support.h"
namespace surge_mm::exciter
{
using namespace surge_mm::exciter;
using namespace chowdsp;
namespace mech = sst::basic_blocks::mechanics;
namespace
{
constexpr double low_freq = 500.0;
constexpr double high_freq = 10000.0;
constexpr double q_val = 0.7071;
} // namespace
class ExciterKernel : public ExciterBase
{
  public:
    enum exciter_params
    {
        exciter_drive = 0,
        exciter_tone,
        exciter_att,
        exciter_rel,
        exciter_mix,

        exciter_num_ctrls,
    };

    ExciterKernel(ExciterStorage *storage, ExciterSettings *fxdata, ExciterValue *pd);
    ~ExciterKernel();

    const char *get_effectname()
    {
        return "Exciter";
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
        os.reset();
        drive_gain.set_target(0);
        drive_gain.instantize();
        wet_gain.set_target(0);
        wet_gain.instantize();
        init();
    }
    void set_params();

    inline void process_sample(float &l, float &r)
    {
        toneFilter.process_sample(l, r, l, r);
        auto levelL = levelDetector.process_sample(l, 0);
        auto levelR = levelDetector.process_sample(r, 0);
        l = std::tanh(l) * levelL;
        r = std::tanh(r) * levelR;
    }

    Oversampling<1, BLOCK_SIZE> os;
    BiquadFilter toneFilter;
    LevelDetector levelDetector;

    lipol_ps_blocksz wet_gain alignas(16), drive_gain alignas(16);
    float dryL alignas(16)[BLOCK_SIZE], dryR alignas(16)[BLOCK_SIZE];
};

inline ExciterKernel::ExciterKernel(ExciterStorage *storage, ExciterSettings *fxdata,
                                    ExciterValue *pd)
    : ExciterBase(storage, fxdata, pd), toneFilter(storage)
{
    wet_gain.set_blocksize(BLOCK_SIZE);
    drive_gain.set_blocksize(BLOCK_SIZE);
}

inline ExciterKernel::~ExciterKernel()
{
}

inline void ExciterKernel::init()
{
    toneFilter.suspend();
    toneFilter.coeff_HP(M_PI, q_val);
    toneFilter.coeff_instantize();

    levelDetector.reset(storage->samplerate);

    drive_gain.set_target(1.0f);
    wet_gain.set_target(0.0f);
}

inline void ExciterKernel::process(float *dataL, float *dataR)
{
    set_params();

    // copy dry signal
    mech::copy_from_to<BLOCK_SIZE>(dataL, dryL);
    mech::copy_from_to<BLOCK_SIZE>(dataR, dryR);

    drive_gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
    os.upsample(dataL, dataR);

    for (int k = 0; k < os.getUpBlockSize(); k++)
        process_sample(os.leftUp[k], os.rightUp[k]);

    os.downsample(dataL, dataR);

    // dry/wet process
    wet_gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
    mech::add_block<BLOCK_SIZE>(dataL, dryL);
    mech::add_block<BLOCK_SIZE>(dataR, dryR);
}

inline void ExciterKernel::set_params()
{
    // "Tone" param
    auto cutoff = low_freq * std::pow(high_freq / low_freq, clamp01(*pd_float[exciter_tone]));
    cutoff = limit_range(cutoff, 10.0, storage->samplerate * 0.48);
    auto omega_factor = storage->samplerate_inv * 2.0 * M_PI / (double)os.getOSRatio();
    toneFilter.coeff_HP(cutoff * omega_factor, q_val);

    // "Drive" param
    auto drive_makeup = std::pow(0.2f, 1.f - clamp01(*pd_float[exciter_tone]));
    auto drive = 8.f * std::pow(clamp01(*pd_float[exciter_drive]), 1.5f) * drive_makeup;
    drive_gain.set_target_smoothed(drive);

    // attack/release params
    auto attack_ms = std::pow(2.0f, fxdata->p[exciter_att].displayInfo.b * *pd_float[exciter_att]);
    auto release_ms =
        10.0f * std::pow(2.0f, fxdata->p[exciter_rel].displayInfo.b * *pd_float[exciter_rel]);

    attack_ms = limit_range(attack_ms, 2.5f, 40.0f);
    release_ms = limit_range(release_ms, 25.0f, 400.0f);

    levelDetector.set_attack_time(attack_ms);
    levelDetector.set_release_time(release_ms);

    // "Mix" param
    wet_gain.set_target_smoothed(clamp01(*pd_float[exciter_mix]));
}

inline void ExciterKernel::suspend()
{
    init();
}
} // namespace surge_mm::exciter
