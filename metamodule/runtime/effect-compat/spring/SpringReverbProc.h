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

#ifndef SURGE_MM_SPRING_SPRING_REVERB_SPRINGREVERBPROC_H
#define SURGE_MM_SPRING_SPRING_REVERB_SPRINGREVERBPROC_H

#include <functional>

#include "spring_support.h"
#include "StateVariableFilter.h"

#include "ReflectionNetwork.h"
#include "SchroederAllpass.h"

namespace surge_mm::spring
{
template <bool Compact = true, bool Guard = true> class SpringReverbProc
{
  public:
    SpringReverbProc();

    struct Params
    {
        float size = 0.5f;
        float decay = 0.5f;
        float reflections = 1.0f;
        float spin = 0.5f;
        float damping = 0.5f;
        float chaos = 0.0f;
        bool shake = false;
    };

    void prepare(float sampleRate, int samplesPerBlock);
    void setParams(const Params &params, int numSamples);
    void processBlock(float *left, float *right, const int numSamples);

  public:
    DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd> delay{Compact ? 32768 : (1 << 18)};
    float feedbackGain;

    StateVariableFilter<float, Guard> dcBlocker;

    static constexpr int allpassStages = 16;
    using VecType = SpringVector;
    using APFCascade = std::array<SchroederAllpass<VecType, 2, Compact>, allpassStages>;
    APFCascade vecAPFs;

    bool coefficientsValid{false};
    Params previousParams{};
    float cachedT60{}, cachedBaseDelay{};
    bool nativeReset{true};
    unsigned seed{1};
    std::minstd_rand generator{1};
    std::uniform_real_distribution<float> distribution{0, 1};
    float urng01()
    {
        return distribution(generator);
    }
    SmoothedValue<float, ValueSmoothingTypes::Linear> chaosSmooth;

    // SIMD register for parallel allpass filter
    // format:
    //   [0]: y_left (feedforward path output)
    //   [1]: z_left (feedback path)
    //   [2-3]: same for right channel
    float simdState alignas(16)[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fs = 48000.0f;

    StateVariableFilter<float, Guard> lpf;

    ReflectionNetwork<Compact> reflectionNetwork; // early reflections

    int shakeCounter = -1;
    std::vector<float> shakeBuffer;
    int shakeBufferSize = 0;
    float shortShakeBuffer[BLOCK_SIZE];
};
} // namespace surge_mm::spring

#endif // SURGE_MM_SPRING_SPRING_REVERB_SPRINGREVERBPROC_H

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
#include <random>

#include "sst/basic-blocks/dsp/FastMath.h"

namespace
{
constexpr float smallShakeSeconds = 0.0005f;
constexpr float largeShakeSeconds = 0.001f;
} // namespace

namespace surge_mm::spring
{
template <bool Compact, bool Guard> SpringReverbProc<Compact, Guard>::SpringReverbProc()
{
    shakeBuffer.reserve(600);
}

template <bool Compact, bool Guard>
void SpringReverbProc<Compact, Guard>::prepare(float sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    coefficientsValid = false;
    if (nativeReset)
    {
        generator.seed(seed);
        distribution.reset();
        feedbackGain = 0;
        chaosSmooth.setCurrentAndTargetValue(0);
    }

    delay.prepare({sampleRate, (uint32_t)samplesPerBlock, 2});

    dcBlocker.prepare(sampleRate, 2);
    dcBlocker.setCutoffFrequency(40.0f);

    for (auto &apf : vecAPFs)
        apf.prepare(sampleRate);

    lpf.prepare(sampleRate, 2);

    reflectionNetwork.prepare(sampleRate, samplesPerBlock);

    chaosSmooth.reset((double)sampleRate, 0.05);

    shakeCounter = -1;
    shakeBufferSize = int(fs * largeShakeSeconds * 3.0f) + BLOCK_SIZE;
    shakeBuffer.resize(shakeBufferSize, 0.0f);
    if (nativeReset)
        std::fill(shakeBuffer.begin(), shakeBuffer.end(), 0.f);

    for (auto &val : simdState)
        val = 0.0f;
}

template <bool Compact, bool Guard>
void SpringReverbProc<Compact, Guard>::setParams(
    const typename SpringReverbProc<Compact, Guard>::Params &params, int numSamples)
{
    auto msToSamples = [this](float ms) { return (ms / 1000.0f) * fs; };

    if (params.shake && shakeCounter < 0) // start shaking
    {
        float shakeAmount = urng01();
        float shakeSeconds =
            smallShakeSeconds + (largeShakeSeconds - smallShakeSeconds) * shakeAmount;
        shakeSeconds *= 1.0f + 0.5f * params.size;
        shakeCounter = int(fs * shakeSeconds);

        shakeBufferSize = shakeCounter + BLOCK_SIZE;
        std::fill(shakeBuffer.begin(), shakeBuffer.end(), 0.0f);
        for (int i = 0; i < shakeCounter; ++i)
            shakeBuffer[i] =
                2.0f * std::sin(2.0f * (float)M_PI * (float)i / (2.0f * (float)shakeCounter));
    }
    else if (!params.shake && shakeCounter == 0) // reset shake for next time
    {
        shakeCounter = -1;
    }

    const bool sizeChanged = !coefficientsValid || params.size != previousParams.size;
    const bool decayChanged = sizeChanged || params.decay != previousParams.decay;
    const bool dampingChanged = !coefficientsValid || params.damping != previousParams.damping;
    float t60Seconds, delaySamples;
    if (!Compact || decayChanged)
    {
        constexpr float lowT60 = 0.5f;
        constexpr float highT60 = 4.5f;
        const auto decayCorr = 0.7f * (1.0f - params.size * params.size);
        t60Seconds = lowT60 * std::pow(highT60 / lowT60, 0.95f * params.decay - decayCorr);

        delaySamples = 1000.0f + std::pow(params.size * 0.099f, 1.0f) * fs;

        cachedT60 = t60Seconds;
        cachedBaseDelay = delaySamples;
    }
    else
    {
        t60Seconds = cachedT60;
        delaySamples = cachedBaseDelay;
    }
    chaosSmooth.setTargetValue(urng01() * delaySamples * 0.07f);
    delaySamples += std::pow(params.chaos, 3.0f) * chaosSmooth.skip(numSamples);
    delay.setDelay(delaySamples);

    feedbackGain = std::pow(0.001f, delaySamples / (t60Seconds * fs));

    if (!Compact || sizeChanged || params.spin != previousParams.spin)
    {
        auto apfG = 0.5f - 0.4f * params.spin;
        float apfGVec alignas(16)[4] = {apfG, -apfG, apfG, -apfG};
        VecType apfGVecAsVecType = 0;
        for (int i = 0; i < 4; ++i)
            apfGVecAsVecType[i] = apfGVec[i];
        for (auto &apf : vecAPFs)
            apf.setParams(msToSamples(0.35f + 3.0f * params.size), apfGVecAsVecType);
    }
    if (!Compact || dampingChanged)
    {
        constexpr float dampFreqLow = 4000.0f;
        constexpr float dampFreqHigh = 18000.0f;
        auto dampFreq = dampFreqLow * std::pow(dampFreqHigh / dampFreqLow, 1.0f - params.damping);
        lpf.setCutoffFrequency(dampFreq);
    }
    auto reflSkew = 0.95f * params.reflections * params.reflections;
    if (!Compact || decayChanged || dampingChanged ||
        params.reflections != previousParams.reflections)
        reflectionNetwork.setParams(params.size, t60Seconds, reflSkew, params.damping);
    previousParams = params;
    coefficientsValid = true;
}

template <bool Compact, bool Guard>
void SpringReverbProc<Compact, Guard>::processBlock(float *left, float *right, const int numSamples)
{
    std::fill(shortShakeBuffer, shortShakeBuffer + BLOCK_SIZE, 0.0f);

    // add shakeBuffer
    if (shakeCounter > 0)
    {
        int startSample = shakeBufferSize - shakeCounter - BLOCK_SIZE;
        std::copy(shakeBuffer.begin() + startSample, shakeBuffer.begin() + startSample + numSamples,
                  shortShakeBuffer);
        shakeCounter = std::max(shakeCounter - numSamples, 0);
    }

    auto doSpringInput = [this](int ch, float input, int n) -> float {
        auto output = sst::basic_blocks::dsp::fasttanh(input - feedbackGain * delay.popSample(ch));
        return dcBlocker.template processSample<StateVariableFilterType::Highpass>(ch, output) +
               shortShakeBuffer[n];
    };

    auto doAPFProcess = [&]() {
        auto yVec = VecType::fromRawArray(simdState);
        for (auto &apf : vecAPFs)
            yVec = apf.processSample(yVec);
        yVec.copyToRawArray(simdState);
    };

    auto doSpringOutput = [&](int ch) {
        auto chIdx = 2 * ch;
        delay.pushSample(ch, simdState[1 + chIdx]);

        simdState[chIdx] -= reflectionNetwork.popSample(ch);
        reflectionNetwork.pushSample(ch, simdState[chIdx]);

        constexpr auto filterType = StateVariableFilterType::Lowpass;
        simdState[chIdx] = lpf.template processSample<filterType>(ch, simdState[chIdx]);
    };

    for (int n = 0; n < numSamples; ++n)
    {
        simdState[0] = doSpringInput(0, left[n], n);
        simdState[2] = doSpringInput(1, right[n], n);

        doAPFProcess();

        doSpringOutput(0);
        doSpringOutput(1);

        left[n] = simdState[0];      // write to output
        simdState[1] = simdState[0]; // write to feedback path
        right[n] = simdState[2];
        simdState[3] = simdState[2];
    }
}

} // namespace surge_mm::spring
