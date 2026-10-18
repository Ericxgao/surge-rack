// SPDX-License-Identifier: GPL-3.0-or-later
// Mode/clock routing adapted from src/QuadLFO.h and src/XTModule.h (Surge Synth Team).
#include "quad_lfo_dsp.h"
#include "modulation_clock.h"
#include <functional>
#include <memory>
#include "globals.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"
namespace surge_mm
{
namespace
{
float tempo_round(float rate)
{
    float whole = std::floor(rate), fraction = std::exp2(rate - whole);
    return whole + (fraction > 1.41f    ? std::log2(1.5f)
                    : fraction > 1.167f ? std::log2(4.f / 3)
                                        : 0);
}
float ratio(float value)
{
    constexpr float values[]{-32, -24, -16, -12, -8, -7, -6, -5, -4, -3, -2, 1,
                             2,   3,   4,   5,   6,  7,  8,  12, 16, 24, 32};
    int index = std::clamp(int(std::round(value * 23 + .5f)) - 1, 0, 22);
    float r = values[index];
    return (r < 0 ? -1 : 1) * std::log2(std::abs(r));
}
} // namespace
struct QuadLFO
{
    using LFO = sst::basic_blocks::modulators::SimpleLFO<EnvelopeClock, 8>;
    EnvelopeClock clock;
    std::array<std::array<std::unique_ptr<LFO>, 4>, 4> lfo;
    Edge triggers[4][4]{}, clock_edge;
    std::array<unsigned, 4> channels{1, 1, 1, 1};
    std::array<float, 4> offsets{};
    int position{}, last_mode{-1}, last_clock_style{-1};
    int clock_samples{-1};
    float tempo_ratio{1};
    bool synced{}, rack_compat{};
    uint32_t seed{};
    struct RackState
    {
        struct Trigger
        {
            unsigned state{2};
            bool process(float v)
            {
                if (state == 0 && v >= 1)
                {
                    state = 1;
                    return true;
                }
                if (state == 1 && v <= 0)
                    state = 0;
                else if (state == 2)
                {
                    if (v >= 1)
                        state = 1;
                    else if (v <= 0)
                        state = 0;
                }
                return false;
            }
        };
        Trigger triggers[4][4], clock;
        int elapsed{-1};
        float ratio{1}, last_bpm{-1}, last_volts{-11};
        bool bpm_connected{}, synced{};
    } rack;
    QuadLFO()
    {
        seed = uint32_t(std::rand());
        for (unsigned stage = 0; stage < 4; ++stage)
            for (unsigned lane = 0; lane < 4; ++lane)
                lfo[stage][lane] = std::make_unique<LFO>(&clock, seed + stage * 7919 + lane * 313);
    }
};
QuadLFO *create_quad_lfo()
{
    return new QuadLFO;
}
void destroy_quad_lfo(QuadLFO *engine)
{
    delete engine;
}
void quad_lfo_rack_mode(QuadLFO *p, bool enabled)
{
    p->rack_compat = enabled;
    if (enabled)
    {
        p->rack.ratio = 1.f / 120;
    }
}
void reset_quad_lfo(QuadLFO *e)
{
    e->position = 0;
    e->last_mode = e->last_clock_style = -1;
    e->clock_samples = -1;
    e->tempo_ratio = 1;
    e->synced = false;
    e->clock_edge = {};
    e->rack = {};
    e->channels.fill(1);
    e->offsets.fill(0);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < 4; ++c)
        {
            auto *lfo = e->lfo[i][c].get();
            lfo->~SimpleLFO();
            new (lfo) QuadLFO::LFO(&e->clock, e->seed + i * 7919 + c * 313);
            e->triggers[i][c] = {};
        }
}
void set_quad_lfo_samplerate(QuadLFO *engine, float sr)
{
    if (std::isfinite(sr) && sr >= 8000 && sr <= 192000)
    {
        engine->clock.set_samplerate(sr);
        engine->clock_samples = -1;
        engine->clock_edge = {};
        if (engine->rack_compat)
            quad_lfo_rack_mode(engine, true);
    }
}
static void process_rack_quad_lfo(QuadLFO *e, const std::array<float, 19> &p,
                                  const std::array<PolyValues, 12> &in,
                                  const std::array<unsigned, 4> &counts,
                                  std::array<PolyValues, 4> &out, std::array<unsigned, 4> &channels)
{
    int mode = int(std::round(p[16] * 4));
    auto &r = e->rack;
    bool reset_amplitude = false;
    if (!mode || !counts[1])
    {
        if (r.elapsed >= 0 || r.bpm_connected)
        {
            r.synced = false;
            reset_amplitude = true;
        }
        r.elapsed = -1;
        r.bpm_connected = false;
    }
    else if (p[18] > .5f)
    {
        if (!r.bpm_connected)
        {
            r.synced = true;
            reset_amplitude = true;
        }
        r.bpm_connected = true;
        if (in[1][0] != r.last_volts)
            r.ratio = float(std::pow(2.0, in[1][0]));
        r.last_volts = in[1][0];
    }
    else
    {
        if (r.clock.process(in[1][0]))
        {
            if (r.elapsed > 0 && r.elapsed < e->clock.samplerate * 6)
            {
                float bpm = 60 * e->clock.samplerate / r.elapsed;
                if (std::abs(bpm - std::round(bpm)) < .015f)
                    bpm = std::round(bpm);
                if (bpm != r.last_bpm)
                    r.ratio = bpm / 120;
                r.last_bpm = bpm;
            }
            else
            {
                r.synced = true;
                reset_amplitude = true;
            }
            r.elapsed = 0;
        }
        if (r.elapsed >= 0 && r.elapsed < 2147483647)
            ++r.elapsed;
    }
    if (reset_amplitude)
        for (auto &stage : e->lfo)
            for (auto &lfo : stage)
                lfo->setAmplitude(1);
    if (e->position == 0)
    {
        if (mode != e->last_mode)
            for (auto &stage : e->lfo)
                for (auto &lfo : stage)
                    lfo->setAmplitude(1);
        e->last_mode = mode;
        unsigned forced = unsigned(std::round(p[17] * 4));
        bool retrigger[4]{};
        for (unsigned stage = 0; stage < 4; ++stage)
        {
            e->channels[stage] = forced ? forced : std::clamp(counts[mode ? 0 : stage], 1u, 4u);
            int shape = int(std::round(p[8 + stage] * 7));
            e->offsets[stage] = p[12 + stage] < .5f || shape == 7 ? 1 : 0;
            for (unsigned c = 0; c < e->channels[stage]; ++c)
            {
                auto &lfo = *e->lfo[stage][c];
                bool trigger = false;
                if (!mode)
                    trigger = counts[stage] && r.triggers[stage][c].process(in[stage][c]);
                else
                {
                    if (stage == 0)
                        retrigger[c] = counts[0] && r.triggers[0][c].process(in[0][c]);
                    trigger = retrigger[c];
                }
                if (mode && counts[2] && in[2][c] > 2)
                {
                    lfo.freeze();
                    continue;
                }
                float v[8];
                for (unsigned i = 0; i < 8; ++i)
                    v[i] = i < 4 ? p[i] + in[4 + i][c] / 5.f
                                 : (p[i] * 2 - 1) + (in[4 + i][c] / 5.f) * 2;
                float rate = v[mode ? 0 : stage] * 13 - 5, deform = v[4 + stage];
                if (mode && r.synced)
                    rate = float(tempo_round(rate - 1) + std::log2(double(r.ratio)));
                if (mode == 1 && stage)
                {
                    constexpr float ratios[12]{2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32};
                    int index = std::clamp(int(std::round(v[stage] * 23 + .5)), 0, 23);
                    float factor = index < 12    ? -ratios[11 - index]
                                   : index == 12 ? 0
                                                 : ratios[index - 13];
                    if (factor != 0)
                        rate = float(rate +
                                     (factor < 0 ? -1 : 1) * std::log2(double(std::abs(factor))));
                }
                if (mode == 2 && stage)
                {
                    lfo.setAmplitude(v[stage]);
                    lfo.applyPhaseOffset(.25 * stage);
                }
                if (mode == 3 && stage)
                    lfo.applyPhaseOffset(v[stage]);
                if (mode == 4)
                {
                    rate += v[4] * stage;
                    float spread = v[5] * .25 * stage;
                    if (spread > 1)
                        spread -= 1;
                    if (spread < 0)
                        spread += 1;
                    float phase = v[1] + (stage ? spread : 0);
                    deform = stage ? std::clamp(v[2] * 2 - 1 + v[6] * stage / 3, -1.f, 1.f)
                                   : v[2] * 2 - 1;
                    float amplitude = stage ? std::clamp(v[3] * 2 - 1 + v[7] * stage / 3, -1.f, 1.f)
                                            : v[3] * 2 - 1;
                    if (trigger)
                        lfo.attack(shape);
                    lfo.applyPhaseOffset(phase);
                    lfo.setAmplitude(amplitude);
                }
                else if (trigger)
                    lfo.attack(shape);
                lfo.process_block(rate, deform, shape, mode && counts[3] && in[3][c] > 2);
            }
        }
    }
    channels = e->channels;
    out = {};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < channels[i]; ++c)
            out[i][c] = (e->lfo[i][c]->outputBlock[e->position] + e->offsets[i]) * 5;
    e->position = (e->position + 1) % 8;
}
void process_quad_lfo(QuadLFO *e, const std::array<float, 19> &p,
                      const std::array<PolyValues, 12> &in, const std::array<unsigned, 4> &counts,
                      std::array<PolyValues, 4> &out, std::array<unsigned, 4> &channels,
                      bool rack_compat)
{
    if (rack_compat)
    {
        process_rack_quad_lfo(e, p, in, counts, out, channels);
        return;
    }
    int mode = int(std::round(p[16] * 4)), style = int(std::round(p[18]));
    if (mode == 0 || !counts[1] || style != e->last_clock_style)
    {
        e->synced = false;
        e->clock_samples = -1;
        e->clock_edge = {};
    }
    e->last_clock_style = style;
    if (mode && counts[1])
    {
        if (style)
        {
            e->synced = true;
            e->tempo_ratio = std::exp2(std::clamp(in[1][0], -10.f, 10.f));
        }
        else
        {
            if (e->clock_edge.process(in[1][0]))
            {
                if (e->clock_samples > 0 && e->clock_samples < e->clock.samplerate * 6)
                {
                    float bpm = 60 * e->clock.samplerate / e->clock_samples;
                    if (std::abs(bpm - std::round(bpm)) < .015f)
                        bpm = std::round(bpm);
                    e->tempo_ratio = bpm / 120;
                }
                e->synced = true;
                e->clock_samples = 0;
            }
            if (e->clock_samples >= 0 && e->clock_samples < e->clock.samplerate * 6 + 1)
                ++e->clock_samples;
        }
    }
    if (e->position == 0)
    {
        int forced = int(std::round(p[17] * 4));
        for (unsigned stage = 0; stage < 4; ++stage)
        {
            e->channels[stage] = forced ? forced : std::clamp(counts[mode ? 0 : stage], 1u, 4u);
            int shape = int(std::round(p[8 + stage] * 7));
            e->offsets[stage] = (p[12 + stage] < .5f || shape == 7) ? 1 : 0;
            for (unsigned lane = 0; lane < 4; ++lane)
            {
                auto &lfo = *e->lfo[stage][lane];
                if (mode != e->last_mode)
                {
                    lfo.setAmplitude(1);
                    lfo.applyPhaseOffset(0);
                }
                if (lane >= e->channels[stage])
                {
                    lfo.attack(shape);
                    lfo.lastTarget = 0;
                    lfo.rndTrigCountdown = 0;
                    e->triggers[stage][lane] = {};
                    continue;
                }
                bool reset = e->triggers[stage][lane].process(in[mode ? 0 : stage][lane]);
                if (mode && in[2][lane] > 2)
                {
                    lfo.freeze();
                    continue;
                }
                float v[8];
                for (unsigned control = 0; control < 8; ++control)
                {
                    v[control] = std::clamp(p[control] + in[4 + control][lane] / 5.f, 0.f, 1.f);
                    if (control >= 4)
                        v[control] = v[control] * 2 - 1;
                }
                float rate = v[mode ? 0 : stage] * 13 - 5;
                if (e->synced)
                    rate = tempo_round(rate - 1) + std::log2(e->tempo_ratio);
                float phase = 0, amplitude = 1, deform = v[4 + stage];
                if (mode == 1 && stage)
                    rate += ratio(v[stage]);
                if (mode == 2 && stage)
                {
                    phase = .25f * stage;
                    amplitude = v[stage];
                }
                if (mode == 3 && stage)
                    phase = v[stage];
                if (mode == 4)
                {
                    rate += v[4] * stage;
                    phase = v[1] + v[5] * .25f * stage;
                    deform = std::clamp(v[2] * 2 - 1 + v[6] * stage / 3, -1.f, 1.f);
                    amplitude = std::clamp(v[3] * 2 - 1 + v[7] * stage / 3, -1.f, 1.f);
                }
                if (reset)
                    lfo.attack(shape);
                lfo.applyPhaseOffset(phase);
                lfo.setAmplitude(amplitude);
                // Keep phase increments below half a cycle per control block.
                // Extreme clock/ratio combinations otherwise overrun SimpleLFO's single-wrap phase handling.
                rate = std::clamp(rate, -15.f, std::log2(e->clock.samplerate / 16));
                lfo.process_block(rate, deform, shape, mode && in[3][lane] > 2);
            }
        }
        e->last_mode = mode;
    }
    channels = e->channels;
    out = {};
    for (unsigned stage = 0; stage < 4; ++stage)
        for (unsigned lane = 0; lane < channels[stage]; ++lane)
            out[stage][lane] =
                (e->lfo[stage][lane]->outputBlock[e->position] + e->offsets[stage]) * 5;
    e->position = (e->position + 1) % 8;
}
} // namespace surge_mm
