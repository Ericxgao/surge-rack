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
#include "chow_support.h"
namespace surge_mm::chow
{
using namespace surge_mm::chow;
using namespace chowdsp;
class ChowKernel : public ChowBase
{
  public:
    enum chow_params
    {
        chow_thresh = 0,
        chow_ratio,
        chow_flip,

        chow_mix,

        chow_num_ctrls,
    };

    ChowKernel(ChowStorage *storage, ChowSettings *fxdata, ChowValue *pd);
    ~ChowKernel();

    const char *get_effectname()
    {
        return "CHOW";
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
        thresh_smooth.setCurrentAndTargetValue(1);
        ratio_smooth.setCurrentAndTargetValue(1);
        makeup.set_target(0);
        makeup.instantize();
        mix.set_target(0);
        mix.instantize();
        cur_os = true;
        init();
    }
    void set_params();
    void process_block(float *dataL, float *dataR);
    void process_block_os(float *dataL, float *dataR);

    enum
    {
        SmoothSteps = 200,
    };

    bool cur_os = true;
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];
    lipol_ps_blocksz makeup alignas(16), mix alignas(16);
    Oversampling<2, BLOCK_SIZE> os;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> thresh_smooth, ratio_smooth;
};

static inline float chowProcess(float x, const float threshGain, const float ratio, const bool flip)
{
    float y = x;

    if (!flip && x > threshGain)
    {
        y = threshGain;
        y += (x - threshGain) / ratio;
    }
    else if (flip && x < -threshGain)
    {
        y = -threshGain;
        y += (x + threshGain) / ratio;
    }

    return y;
}
inline ChowKernel::ChowKernel(ChowStorage *storage, ChowSettings *fxdata, ChowValue *pd)
    : ChowBase(storage, fxdata, pd)
{
    makeup.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
}

inline ChowKernel::~ChowKernel()
{
}

inline void ChowKernel::init()
{
    os.reset();
    makeup.set_target(1.0f);
    mix.set_target(1.0f);

    thresh_smooth.reset(SmoothSteps);
    ratio_smooth.reset(SmoothSteps);

    mix.instantize();
}

inline void ChowKernel::process(float *dataL, float *dataR)
{
    set_params();

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        L[i] = dataL[i];
        R[i] = dataR[i];
    }

    if (cur_os)
        process_block_os(dataL, dataR);
    else
        process_block(dataL, dataR);

    makeup.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(clamp01(*pd_float[chow_mix]));
    mix.fade_2_blocks_inplace(L, dataL, R, dataR, BLOCK_SIZE_QUAD);
    std::copy_n(L, BLOCK_SIZE, dataL);
    std::copy_n(R, BLOCK_SIZE, dataR);
}

inline void ChowKernel::set_params()
{
    auto thresh_clamped = limit_range(*pd_float[chow_thresh], fxdata->p[chow_thresh].val_min.f,
                                      fxdata->p[chow_thresh].val_max.f);
    const auto threshGain = storage->db_to_linear(thresh_clamped);

    // hand-tuned from several years ago...
    const auto ratio = limit_range(*pd_float[chow_ratio], fxdata->p[chow_ratio].val_min.f,
                                   fxdata->p[chow_ratio].val_max.f);
    auto makeup_gain =
        storage->db_to_linear((thresh_clamped / 12.f) * ((1.0f / ratio) - 1.0f) - 1.0f);

    makeup_gain *= cur_os ? 4.0f : 1.0f;
    makeup.set_target_smoothed(makeup_gain);

    thresh_smooth.setTargetValue(threshGain);
    ratio_smooth.setTargetValue(ratio);
}

inline void ChowKernel::process_block(float *dataL, float *dataR)
{
    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float cur_thresh = thresh_smooth.getNextValue();
        float cur_ratio = ratio_smooth.getNextValue();
        bool cur_flip = fxdata->p[chow_flip].val.b;

        dataL[k] = chowProcess(dataL[k], cur_thresh, cur_ratio, cur_flip);
        dataR[k] = chowProcess(dataR[k], cur_thresh, cur_ratio, cur_flip);
    }
}

inline void ChowKernel::process_block_os(float *dataL, float *dataR)
{
    os.upsample(dataL, dataR);

    float cur_thresh{0.f}, cur_ratio{10.f};
    bool cur_flip{false};

    for (int k = 0; k < os.getUpBlockSize(); k++)
    {
        if (k % os.getOSRatio() == 0)
        {
            cur_thresh = thresh_smooth.getNextValue();
            cur_ratio = ratio_smooth.getNextValue();
            cur_flip = fxdata->p[chow_flip].val.b;
        }

        os.leftUp[k] = chowProcess(os.leftUp[k], cur_thresh, cur_ratio, cur_flip);
        os.rightUp[k] = chowProcess(os.rightUp[k], cur_thresh, cur_ratio, cur_flip);
    }

    os.downsample(dataL, dataR);
}

inline void ChowKernel::suspend()
{
    init();
}
} // namespace surge_mm::chow
