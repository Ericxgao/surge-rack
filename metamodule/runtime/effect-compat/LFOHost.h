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
namespace surge_mm::lfo
{
inline float SurgeStorage::lookup_waveshape_warp(sst::waveshapers::WaveshaperType entry, float x)
{
    x *= 256.f;
    x += 512.f;

    int e = (int)x;
    float a = x - (float)e;

    const auto &waveshapers = sst::waveshapers::globalWaveshaperTables.waveshapers[(int)entry];
    return (1 - a) * waveshapers[e & 0x3ff] + a * waveshapers[(e + 1) & 0x3ff];
}
inline float SurgeStorage::envelope_rate_linear_nowrap(float x)
{
    x *= 16.f;
    x += 256.f;
    int e = limit_range((int)x, 0, 0x1ff - 1);
    ;
    float a = x - (float)e;

    return (1 - a) * table_envrate_linear[e & 0x1ff] + a * table_envrate_linear[(e + 1) & 0x1ff];
}
} // namespace surge_mm::lfo
