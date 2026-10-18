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
namespace surge_mm::ensemble
{
inline float EnsembleStorage::lookup_waveshape(sst::waveshapers::WaveshaperType entry, float x)
{
    x *= 32.f;
    x += 512.f;
    int e = (int)x;
    float a = x - (float)e;

    if (e > 0x3fd)
        return 1;
    if (e < 1)
        return -1;

    const auto &waveshapers = sst::waveshapers::globalWaveshaperTables.waveshapers[(int)entry];
    return (1 - a) * waveshapers[e & 0x3ff] + a * waveshapers[(e + 1) & 0x3ff];
}
inline void EnsembleBase::applyWidth(float *__restrict L, float *__restrict R,
                                     lipol_ps_blocksz &width)
{
    namespace sdsp = sst::basic_blocks::dsp;
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    sdsp::encodeMS<BLOCK_SIZE>(L, R, M, S);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    sdsp::decodeMS<BLOCK_SIZE>(M, S, L, R);
}
} // namespace surge_mm::ensemble
