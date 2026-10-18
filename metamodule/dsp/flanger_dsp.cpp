// SPDX-License-Identifier: GPL-3.0-or-later
#include "flanger_dsp.h"
#include "fx_clock.h"
#include "globals.h"
#include "effect_config.h"
#include <sst/effects/Flanger.h>
#include <memory>
#include <random>
namespace surge_mm
{
namespace
{
struct RackRandom
{
    std::minstd_rand engine;
    std::uniform_real_distribution<float> distribution{0.f, 1.f};
    float next()
    {
        return distribution(engine);
    }
};
struct FlangerGlobal : EffectGlobal
{
    RackRandom *rack_random{};
};
struct FlangerConfig : EffectConfig
{
    using GlobalStorage = FlangerGlobal;
    static float rand01(GlobalStorage *s)
    {
        return s->rack_random ? s->rack_random->next() : EffectConfig::rand01(s);
    }
    static float envelopeRateLinear(GlobalStorage *s, float f)
    {
        return EffectConfig::envelopeRateLinear(s, f);
    }
    static float temposyncRatio(GlobalStorage *s, EffectStorage *e, int i)
    {
        return EffectConfig::temposyncRatio(s, e, i);
    }
    static double sampleRate(GlobalStorage *s)
    {
        return EffectConfig::sampleRate(s);
    }
    static float noteToPitch(GlobalStorage *s, float f)
    {
        return EffectConfig::noteToPitch(s, f);
    }
    static float noteToPitchIgnoringTuning(GlobalStorage *s, float f)
    {
        return EffectConfig::noteToPitchIgnoringTuning(s, f);
    }
    static float noteToPitchInv(GlobalStorage *s, float f)
    {
        return EffectConfig::noteToPitchInv(s, f);
    }
    static float dbToLinear(GlobalStorage *s, float f)
    {
        return EffectConfig::dbToLinear(s, f);
    }
};
struct Kernel : sst::effects::flanger::Flanger<FlangerConfig>
{
    using Base = sst::effects::flanger::Flanger<FlangerConfig>;
    using Base::Base;
    void rack_initialize()
    {
        // Original initialize(), excluding immutable table generation. In
        // particular it retains the delay lines, damping and smoothing state.
        for (auto &d : idels)
            d.nativeBounds = false;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < COMBS_PER_CHANNEL; ++i)
            {
                lfophase[c][i] = 1.f * (i + .5 * c) / COMBS_PER_CHANNEL;
                lfosandhtarget[c][i] = 0;
            }
        longphase[0] = 0;
        longphase[1] = .5f;
        haveProcessed = false;
    }
    // Reset mutable state without rebuilding the immutable sine table on audio.
    void clear()
    {
        for (auto &d : idels)
        {
            d.reset();
            d.nativeBounds = true;
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < COMBS_PER_CHANNEL; ++i)
            {
                lfophase[c][i] = 1.f * (i + .5 * c) / COMBS_PER_CHANNEL;
                lfosandhtarget[c][i] = vweights[c][i] = 0;
                lfoval[c][i].reset();
                delaybase[c][i].reset();
            }
        longphase[0] = 0;
        longphase[1] = .5f;
        lpaL = lpaR = 0;
        haveProcessed = false;
        depth.reset();
        mix.reset();
        voices.reset();
        voice_detune.reset();
        voice_chord.reset();
        feedback.reset();
        fb_hf_damping.reset();
        vzeropitch.startValue(0);
        width.set_target(0);
        width.instantize();
    }
};
struct Voice
{
    FlangerGlobal global;
    EffectSettings settings;
    std::array<float, 11> values{};
    Kernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    Voice()
    {
        kernel.initialize();
        kernel.clear();
        (void)FilterTuning::note_to_pitch(0);
    }
    void reset()
    {
        kernel.clear();
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Flanger
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    RackRandom rack_random;
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    std::array<float, 11> minimum{}, range{};
    Flanger()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
        for (int i = 0; i < 11; ++i)
        {
            auto m = voices[0]->kernel.paramAt(i);
            minimum[i] = m.minVal;
            range[i] = m.maxVal - m.minVal;
        }
    }
};
Flanger *create_flanger()
{
    return new Flanger;
}
void destroy_flanger(Flanger *p)
{
    delete p;
}
void flanger_rack_clock(Flanger *p, bool enabled)
{
    p->tempo.set_rack_mode(enabled);
    if (enabled)
    {
        // Rack uses one independent SurgeStorage RNG shared by Sum/Poly engines.
        // Obtain a seed during loading; subsequent audio never uses global rand.
        p->rack_random.engine.seed(std::rand());
        float sr = p->voices[0]->global.samplerate;
        auto fresh = [p, sr](std::unique_ptr<Voice> &v) {
            v = std::make_unique<Voice>();
            v->global.set_samplerate(sr);
            v->global.rack_random = &p->rack_random;
            v->kernel.rack_initialize();
        };
        fresh(p->rack_mono);
        for (auto &v : p->voices)
            fresh(v);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
    else
    {
        for (auto &v : p->voices)
            v->global.rack_random = nullptr;
        if (p->rack_mono)
            p->rack_mono->global.rack_random = nullptr;
    }
}
void reset_flanger(Flanger *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void flanger_samplerate(Flanger *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_flanger(p);
}
void process_flanger(Flanger *p, const std::array<float, 13> &params,
                     const std::array<PolyValues, 11> &cv, const StereoPoly &input,
                     unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[11] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_flanger(p);
        p->channels = channels;
    }
    p->tempo.process(clock, connected, params[12] > .5f, p->voices[0]->global.samplerate);
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                p->voices[c]->kernel.rack_initialize();
                p->rack_created[c] = true;
            }
        p->rack_mono->kernel.rack_initialize();
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                v.kernel.rack_initialize();
            for (auto &b : v.input)
                std::fill(std::begin(b), std::end(b), 0);
            for (auto &b : v.processed)
                std::fill(std::begin(b), std::end(b), 0);
        }
        p->position = 0;
    }
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 11; ++i)
            {
                float scale = i == Kernel::fl_voice_basepitch ? 12.f / 127 : .2f;
                v.values[i] = p->minimum[i] +
                              std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) * p->range[i];
            }
            constexpr auto r = Kernel::fl_rate;
            v.settings.synced[r] = p->tempo.synced;
            v.global.tempo_ratio = p->tempo.ratio;
            if (p->tempo.synced)
                v.values[r] = std::clamp(round_tempo_time(p->minimum[r] + params[r] * p->range[r]) +
                                             cv[r][c] / 5 * p->range[r],
                                         p->minimum[r], p->minimum[r] + p->range[r]);
            if (imported)
                for (unsigned i = 2; i < 11; ++i)
                {
                    // Rack adds float modulation after bounding/snapping the base
                    // controls. Integer Mode/Waveform direct CV stays bounded.
                    float base = p->minimum[i] + params[i] * p->range[i];
                    if (i == r && p->tempo.synced)
                        base = round_tempo_time(base);
                    v.values[i] = base + cv[i][c] *
                                             (i == Kernel::fl_voice_basepitch ? 12.f / 127 : .2f) *
                                             p->range[i];
                }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.processBlock(buffer.processed[0], buffer.processed[1]);
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? p->voices[c]->processed[s][p->position] * 5 : 0;
}
} // namespace surge_mm
