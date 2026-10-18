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
#include "distortion_support.h"
namespace surge_mm::distortion
{
using namespace std;
static constexpr int n_fxws = 8;
static constexpr std::array<sst::waveshapers::WaveshaperType, n_fxws> FXWaveShapers = {
    sst::waveshapers::WaveshaperType::wst_soft, sst::waveshapers::WaveshaperType::wst_hard,
    sst::waveshapers::WaveshaperType::wst_asym, sst::waveshapers::WaveshaperType::wst_sine,
    sst::waveshapers::WaveshaperType::wst_digital, // If you adjust this list above here, you
                                                   // break 1.9 patch compat
    sst::waveshapers::WaveshaperType::wst_ojd, sst::waveshapers::WaveshaperType::wst_fwrectify,
    sst::waveshapers::WaveshaperType::wst_fuzzsoft};
inline float DistortionStorage::lookup_waveshape(sst::waveshapers::WaveshaperType entry, float x)
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
} // namespace surge_mm::distortion
