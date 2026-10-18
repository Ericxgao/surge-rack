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

#ifndef SURGE_MM_SPRING_SPRING_REVERB_REFLECTIONNETWORK_H
#define SURGE_MM_SPRING_SPRING_REVERB_REFLECTIONNETWORK_H

#include <array>

#include "spring_support.h"

#include "Shelf.h"

namespace surge_mm::spring
{
template <bool Compact = true> class ReflectionNetwork
{
  public:
    ReflectionNetwork() = default;

    void prepare(float sampleRate, int samplesPerBlock)
    {
        fs = sampleRate;

        for (unsigned i = 0; i < (Compact ? 1u : 4u); ++i)
            delays[i].prepare({sampleRate, (uint32_t)samplesPerBlock, 2});

        shelfFilter.reset();
    }

    void setParams(float reverbSize, float t60, float mix, float damping)
    {
        constexpr float baseDelaysSec[4] = {0.07f, 0.17f, 0.23f, 0.29f};

        float feedbackArr alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto delaySamples = baseDelaysSec[i] * reverbSize * fs;
            if constexpr (Compact)
                taps[i] = SpringReflectionDelay::makeTap(delaySamples);
            else
                delays[i].setDelay(delaySamples);
            feedbackArr[i] = std::pow(0.001f, delaySamples / (t60 * fs));
            feedbackArr[i] *= 0.23f * mix * (0.735f + 0.235f * reverbSize);
        }

        feedback = _mm_load_ps(feedbackArr);

        float dampDB = -1.0f - 9.0f * damping;
        shelfFilter.calcCoefs(1.0f, (float)dB_to_linear((double)dampDB), 800.0f, fs);
    }

    inline float popSample(int ch) noexcept
    {
        float output alignas(16)[4];
        for (int i = 0; i < 4; ++i)
            if constexpr (Compact)
                output[i] = delays[0].readTap(ch, taps[i]);
            else
                output[i] = delays[i].popSample(ch);
        if constexpr (Compact)
            delays[0].incrementReadPointer(ch);

        auto outVec = _mm_load_ps(output);

        // householder matrix
        constexpr auto householderFactor = -2.0f / (float)4;
        const auto sumXhh = vSum(outVec) * householderFactor;
        outVec = _mm_add_ps(outVec, _mm_load1_ps(&sumXhh));

        outVec = _mm_mul_ps(outVec, feedback);
        return shelfFilter.processSample(vSum(outVec) / (float)4);
    }

    inline void pushSample(int ch, float x) noexcept
    {
        for (unsigned i = 0; i < (Compact ? 1u : 4u); ++i)
            delays[i].pushSample(ch, x);
    }

  private:
    float fs = 48000.0f;

    using ReflectionDelay =
        std::conditional_t<Compact, SpringReflectionDelay,
                           DelayLine<float, chowdsp::DelayLineInterpolationTypes::Lagrange3rd>>;
    std::array<ReflectionDelay, 4> delays{ReflectionDelay{1 << 18}, ReflectionDelay{1 << 18},
                                          ReflectionDelay{1 << 18}, ReflectionDelay{1 << 18}};

    std::array<SpringReflectionDelay::Tap, 4> taps{};
    __m128 feedback;

    ShelfFilter<> shelfFilter;
};
} // namespace surge_mm::spring

#endif // SURGE_MM_SPRING_SPRING_REVERB_REFLECTIONNETWORK_H
