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
#include "SpringReverbProc.h"
namespace surge_mm::spring
{
template <bool Compact = true, bool Guard = true> class SpringKernel : public SpringBase
{
  public:
    enum spring_reverb_params
    {
        spring_reverb_size,
        spring_reverb_decay,
        spring_reverb_reflections,
        spring_reverb_damping,

        spring_reverb_spin,
        spring_reverb_chaos,
        spring_reverb_knock,

        spring_reverb_mix,

        spring_reverb_num_ctrls,
    };

    SpringKernel(SpringStorage *storage, SpringSettings *fxdata, SpringValue *pd);
    ~SpringKernel() = default;

    const char *get_effectname()
    {
        return "Spring Reverb";
    }

    void init();
    void process(float *dataL, float *dataR);
    void suspend();

    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);

  public:
    SpringReverbProc<Compact, Guard> proc;

    lipol_ps_blocksz mix alignas(16), makeup alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];
};

template <bool Compact, bool Guard>
SpringKernel<Compact, Guard>::SpringKernel(SpringStorage *storage, SpringSettings *fxdata,
                                           SpringValue *pd)
    : SpringBase(storage, fxdata, pd)
{
}

template <bool Compact, bool Guard> void SpringKernel<Compact, Guard>::init()
{
    proc.prepare(storage->samplerate, BLOCK_SIZE);

    mix.set_target(1.f);
    mix.instantize();
}

template <bool Compact, bool Guard>
void SpringKernel<Compact, Guard>::process(float *dataL, float *dataR)
{
    proc.setParams(
        {
            clamp01(*pd_float[spring_reverb_size]),
            clamp01(*pd_float[spring_reverb_decay]),
            clamp01(*pd_float[spring_reverb_reflections]),
            clamp01(*pd_float[spring_reverb_spin]),
            clamp01(*pd_float[spring_reverb_damping]),
            clamp01(*pd_float[spring_reverb_chaos]),
            *pd_float[spring_reverb_knock] > 0.5f,
        },
        BLOCK_SIZE);

    mech::copy_from_to<BLOCK_SIZE>(dataL, L);
    mech::copy_from_to<BLOCK_SIZE>(dataR, R);

    proc.processBlock(L, R, BLOCK_SIZE);

    mix.set_target_smoothed(clamp01(*pd_float[spring_reverb_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

template <bool Compact, bool Guard> void SpringKernel<Compact, Guard>::suspend()
{
    init();
}
} // namespace surge_mm::spring
