// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from src/DelayLineByFreqExpanded.h, Surge Synth Team 2019-2024.
#include "tuned_delay_plus_dsp.h"
#include "filter_tuning.h"
#include <cstring>
#include <memory>
#include <random>
#include <sst/utilities/globals.h>
#include <sst/basic-blocks/dsp/SSESincDelayLine.h>
#include <sst/filters/BiquadFilter.h>
namespace surge_mm
{
namespace
{
struct Storage
{
    float samplerate{48000};
    double dsamplerate_inv{1.0 / 48000};
    static float note_to_pitch_ignoring_tuning(float n)
    {
        return FilterTuning::note_to_pitch_ignoring_tuning(n);
    }
};
using Line = sst::basic_blocks::dsp::SSESincDelayLine<16384>;
using Biquad = sst::filters::Biquad::BiquadFilter<Storage, 8>;
const auto &sinc()
{
    static const sst::basic_blocks::tables::SurgeSincTableProvider table;
    return table;
}
struct Voice
{
    Line left{sinc()}, right{sinc()};
    Biquad lp, hp;
    Voice(Storage *s) : lp(s), hp(s)
    {
        lp.suspend();
        hp.suspend();
    }
    void reset()
    {
        left.clear();
        right.clear();
        lp.suspend();
        hp.suspend();
    }
};
constexpr unsigned seed = 0x2468ace1;
} // namespace
struct TunedDelayPlus
{
    Storage storage;
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::default_random_engine random{seed};
    std::uniform_real_distribution<float> distribution{-1.f, 1.f};
    unsigned position{}, channels{1};
    bool use_lp{}, use_hp{};
    float clamp_level{10};
    TunedDelayPlus()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>(&storage);
    }
};
TunedDelayPlus *create_tuned_delay_plus()
{
    return new TunedDelayPlus;
}
void destroy_tuned_delay_plus(TunedDelayPlus *p)
{
    delete p;
}
void reset_tuned_delay_plus(TunedDelayPlus *p)
{
    for (auto &v : p->voices)
        v->reset();
    p->position = 0;
    p->use_lp = p->use_hp = false;
    p->clamp_level = 10;
    p->random.seed(seed);
    p->distribution.reset();
}
void tuned_delay_plus_samplerate(TunedDelayPlus *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    p->storage.samplerate = sr;
    p->storage.dsamplerate_inv = 1.0 / sr;
    reset_tuned_delay_plus(p);
}
void process_tuned_delay_plus(TunedDelayPlus *p, const std::array<float, 12> &params,
                              const std::array<PolyValues, 7> &cv, const StereoPoly &audio,
                              const StereoPoly &feedback, const PolyValues &pitch,
                              const PolyValues &excite, unsigned channels, StereoPoly &out)
{
    if (p->channels != channels)
    {
        reset_tuned_delay_plus(p);
        p->channels = channels;
    }
    std::array<PolyValues, 7> values{};
    PolyValues correction{};
    for (unsigned c = 0; c < channels; ++c)
    {
        for (unsigned i = 0; i < 7; ++i)
        {
            float normalized = params[i];
            if (i)
                normalized += cv[i - 1][c] * .2f;
            // Rack modulation extends beyond knob limits. Preserve that here;
            // the original feedback-law, delay-buffer and write clamps follow.
            values[i][c] = tuned_plus_minimum[i] + normalized * tuned_plus_range[i];
        }
        correction[c] = std::round(std::clamp(params[9] + cv[6][c] * .2f, 0.f, 1.f) * 20);
    }
    if (p->position == 0)
    {
        constexpr float levels[]{20, 10, 5};
        p->clamp_level = levels[int(std::round(std::clamp(params[11], 0.f, 1.f) * 2))];
        bool lp = params[7] > .5f, hp = params[8] > .5f, lp_toggle = lp != p->use_lp,
             hp_toggle = hp != p->use_hp;
        if (lp_toggle)
            for (auto &v : p->voices)
                v->lp.suspend();
        if (hp_toggle)
            for (auto &v : p->voices)
                v->hp.suspend();
        p->use_lp = lp;
        p->use_hp = hp;
        for (unsigned c = 0; c < channels; ++c)
        {
            float note = (values[0][c] + 5) * 12 + pitch[c] * 12;
            auto &v = *p->voices[c];
            // Retain Rack's original 440-Hz-relative Biquad omega calculation,
            // including LP bypass / HP mute above Nyquist and its 8-sample cadence.
            if (lp)
            {
                auto cutoff = note + values[4][c];
                v.lp.coeff_LP2B(v.lp.calc_omega(cutoff / 12.0), .707);
                if (lp_toggle)
                    v.lp.coeff_instantize();
            }
            if (hp)
            {
                auto cutoff = note + values[5][c];
                v.hp.coeff_HP(v.hp.calc_omega(cutoff / 12.0), .707);
                if (hp_toggle)
                    v.hp.coeff_instantize();
            }
        }
    }
    for (unsigned c = 0; c < channels; ++c)
    {
        auto &v = *p->voices[c];
        float note = (values[0][c] + 5) * 12 + pitch[c] * 12;
        auto inverse_l =
            FilterTuning::note_to_pitch_inv_ignoring_tuning(note + values[1][c] * .01) /
            FilterTuning::MIDI_0_FREQ;
        auto inverse_r =
            FilterTuning::note_to_pitch_inv_ignoring_tuning(note + values[2][c] * .01) /
            FilterTuning::MIDI_0_FREQ;
        float time_l = p->storage.samplerate * inverse_l - correction[c],
              time_r = p->storage.samplerate * inverse_r - correction[c];
        time_l = std::clamp(time_l, 12.f, 16372.f);
        time_r = std::clamp(time_r, 12.f, 16372.f);
        out[0][c] = v.left.read(time_l);
        out[1][c] = v.right.read(time_r);
        float gain = values[3][c];
        if (params[10] <= .5f)
        {
            constexpr float base = .67021327021f;
            gain = std::clamp(gain * (1.f - base) + base, 0.f, 1.f);
        }
        float remainder = 1.f - gain;
        gain = 1.f - remainder * remainder;
        float l = gain * feedback[0][c], r = gain * feedback[1][c];
        float excitation = excite[c] * 0.1;
        if (excitation > 1e-5)
        {
            l += excitation * p->distribution(p->random);
            r += excitation * p->distribution(p->random);
        }
        if (p->use_lp || p->use_hp)
        {
            float filtered_l = l, filtered_r = r, mix = values[6][c];
            mix = mix * mix * mix;
            if (p->use_lp)
                v.lp.process_sample(filtered_l, filtered_r, filtered_l, filtered_r);
            if (p->use_hp)
                v.hp.process_sample(filtered_l, filtered_r, filtered_l, filtered_r);
            l = mix * filtered_l + (1 - mix) * l;
            r = mix * filtered_r + (1 - mix) * r;
        }
        v.left.write(std::clamp(audio[0][c] + l, -p->clamp_level, p->clamp_level));
        v.right.write(std::clamp(audio[1][c] + r, -p->clamp_level, p->clamp_level));
    }
    p->position = (p->position + 1) % 8;
    for (unsigned c = channels; c < 4; ++c)
        out[0][c] = out[1][c] = 0;
}
} // namespace surge_mm
