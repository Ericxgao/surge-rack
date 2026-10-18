// SPDX-License-Identifier: GPL-3.0-or-later
// Native adapter for src/LFO.h and the original LFOModulationSource engine.
#include "lfo_dsp.h"
#include "LFOKernel.h"
#include "modulation_clock.h"
namespace surge_mm
{
namespace
{
constexpr std::array<unsigned, 10> offsets{0, 2, 4, 3, 7, 9, 8, 10, 11, 12};
// Rack starts Schmitt triggers unarmed and retains their state on disconnection.
struct RackTrigger
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
struct RackClock
{
    RackTrigger trigger;
    int elapsed{-1};
    float ratio{1}, inverse{1}, last_bpm{-1}, last_volts{-11};
    bool connected{}, synced{};
    void process(float volts, bool patched, bool bpm, float sr)
    {
        if (!patched)
        {
            if (elapsed >= 0 || connected)
                synced = false;
            elapsed = -1;
            connected = false;
            return;
        }
        if (bpm)
        {
            if (!connected)
                synced = true;
            connected = true;
            if (volts != last_volts)
            {
                double r = std::pow(2.0, volts);
                ratio = r;
                inverse = 1.f / r;
            }
            last_volts = volts;
        }
        else
        {
            if (trigger.process(volts))
            {
                if (elapsed > 0 && elapsed < sr * 6)
                {
                    float rate = 60 * sr / elapsed;
                    if (std::abs(rate - std::round(rate)) < .015f)
                        rate = std::round(rate);
                    if (rate != last_bpm)
                    {
                        ratio = rate / 120;
                        inverse = 120 / rate;
                    }
                    last_bpm = rate;
                }
                else
                    synced = true;
                elapsed = 0;
            }
            if (elapsed >= 0 && elapsed < 2147483647)
                ++elapsed;
        }
    }
};
struct Voice
{
    lfo::LFOStorage settings;
    std::array<lfo::pdata, 13> values{};
    lfo::LFOKernel kernel;
    Edge gate_edge, env_edge;
    bool attack{}, env_attack{}, high{}, previous_high{};
    RackTrigger rack_gate, rack_env;
    int prior_phase{-1}, prior_env{-1};
    std::array<unsigned, 3> countdown{};
    std::array<float, 3> previous{}, next{};
};
lfo::Parameter &field(lfo::LFOStorage &s, unsigned id)
{
    lfo::Parameter *fields[]{&s.rate,     &s.shape,    &s.start_phase, &s.magnitude, &s.deform,
                             &s.trigmode, &s.unipolar, &s.delay,       &s.hold,      &s.attack,
                             &s.decay,    &s.sustain,  &s.release};
    return *fields[id];
}
} // namespace
struct LFO
{
    lfo::SurgeStorage storage;
    lfo::StepSequencerStorage steps;
    std::array<Voice, 4> voices;
    TempoClock tempo;
    RackClock rack_clock;
    bool rack_compat{};
    unsigned position{}, channels{1}, pulse_samples{576}, previous_shape{};
    LFO()
    {
        reset_lfo(this);
    }
};
LFO *create_lfo()
{
    return new LFO;
}
void destroy_lfo(LFO *p)
{
    delete p;
}
void lfo_rack_mode(LFO *p, bool enabled)
{
    p->rack_compat = enabled;
    if (enabled)
    {
        p->rack_clock.ratio = 1.f / 120;
        p->rack_clock.inverse = 1 / p->rack_clock.ratio;
    }
}
void reset_lfo(LFO *p)
{
    p->position = 0;
    p->channels = 1;
    p->previous_shape = 0;
    p->tempo = {};
    p->rack_clock = {};
    p->steps = {};
    p->storage.generator.seed(0x2468ace1);
    p->storage.temposyncratio = p->storage.temposyncratio_inv = 1;
    for (auto &v : p->voices)
    {
        v.settings = {};
        v.values = {};
        v.kernel.reset_native(&p->storage, &v.settings, v.values.data(), &p->steps);
        v.kernel.envRetrigMode = lfo::LFOKernel::FROM_LAST;
        v.gate_edge = {};
        v.env_edge = {};
        v.rack_gate = {};
        v.rack_env = {};
        v.attack = v.env_attack = v.high = v.previous_high = false;
        v.prior_phase = v.prior_env = -1;
        v.countdown = {};
        v.previous = v.next = {};
    }
}
void lfo_samplerate(LFO *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    p->storage.set_samplerate(sr);
    p->pulse_samples = unsigned(std::ceil(sr * .012));
    if (p->rack_compat)
        lfo_rack_mode(p, true);
    else
        reset_lfo(p);
}
void process_lfo(LFO *p, std::array<float, lfo_parameter_count> &params,
                 const std::array<PolyValues, 14> &inputs, const std::array<uint8_t, 14> &counts,
                 unsigned channels, std::array<PolyValues, 6> &out, bool rack_compat)
{
    if (channels != p->channels)
    {
        if (rack_compat)
            p->position = 0;
        else
            reset_lfo(p);
        p->channels = channels;
    }
    const bool gate_connected = counts[0] || counts[1], broadcast = params[53] > .5f;
    for (unsigned c = 0; c < channels; ++c)
    {
        auto &v = p->voices[c];
        unsigned lane = broadcast ? 0 : c;
        float gate = lane < counts[0] ? inputs[0][lane] : 0,
              env = lane < counts[1] ? inputs[1][lane] : 0;
        if (rack_compat)
        {
            if (broadcast && c)
            {
                v.attack = p->voices[0].attack;
                v.env_attack = p->voices[0].env_attack;
            }
            else
            {
                if (counts[0] && v.rack_gate.process(gate))
                    v.attack = true;
                if (counts[1] && v.rack_env.process(env))
                    v.env_attack = true;
            }
        }
        else
        {
            if (v.gate_edge.process(gate))
                v.attack = true;
            if (v.env_edge.process(env))
                v.env_attack = true;
        }
        // Imported patches retain Rack's full-gate-only release test in broadcast mode.
        v.high = gate > 2 || (!(rack_compat && broadcast) && env > 2);
    }
    bool synced;
    if (rack_compat)
    {
        p->rack_clock.process(inputs[2][0], counts[2] > 0, params[55] > .5f, p->storage.samplerate);
        p->storage.temposyncratio = p->rack_clock.ratio;
        p->storage.temposyncratio_inv = p->rack_clock.inverse;
        synced = p->rack_clock.synced;
    }
    else
    {
        p->tempo.process(inputs[2][0], counts[2] > 0, params[55] > .5f, p->storage.samplerate);
        p->storage.temposyncratio = p->tempo.ratio;
        p->storage.temposyncratio_inv = 1 / p->tempo.ratio;
        synced = p->tempo.synced;
    }
    if (p->position == 0)
    {
        unsigned shape = unsigned(std::round(params[10] * 7)),
                 deform = std::min(unsigned(std::round(params[12] * 2)),
                                   unsigned(lfo::deform_counts[shape] - 1));
        if (rack_compat && deform != unsigned(std::round(params[12] * 2)))
            params[12] = deform / 2.f;
        unsigned sync = unsigned(std::round(params[13] * 3));
        if (!rack_compat || shape == lfo::lt_stepseq)
        {
            p->steps.loop_start = int(std::round(params[49] * 15));
            p->steps.loop_end = int(std::round(params[50] * 15));
            p->steps.trigmask = 0;
            for (unsigned i = 0; i < 16; ++i)
            {
                p->steps.steps[i] = params[17 + i] * 2 - 1;
                unsigned mask = unsigned(std::round(params[33 + i] * 3));
                if (mask & 1)
                    p->steps.trigmask |= uint64_t(1) << (16 + i);
                if (mask & 2)
                    p->steps.trigmask |= uint64_t(1) << (32 + i);
            }
        }
        unsigned trigger_type[2]{unsigned(std::round(params[15] * 4)),
                                 unsigned(std::round(params[16] * 4))};
        // Rack selects automatic A/B roles from the preceding block's shape.
        for (unsigned i = 0; i < 2; ++i)
            if (rack_compat && p->previous_shape != lfo::lt_stepseq)
                trigger_type[i] = 2 - i;
            else if (!trigger_type[i])
                trigger_type[i] = p->previous_shape == lfo::lt_stepseq ? 3 + i : 2 - i;
        p->previous_shape = shape;
        if (rack_compat)
            for (auto &v : p->voices)
                v.previous = v.next;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &v = p->voices[c];
            auto &s = v.settings;
            v.previous = v.next;
            s.shape.val.i = shape;
            s.unipolar.val.b = params[11] > .5f;
            s.deform.deform_type = deform;
            s.rate.deactivated = counts[3] > 0;
            s.trigmode.val.i = params[14] > .5f ? lfo::lm_random : lfo::lm_keytrigger;
            for (unsigned i = 0; i < 10; ++i)
            {
                auto &f = field(s, offsets[i]);
                f.temposync =
                    synced && ((i == 0 && (sync & 1)) || (i >= 4 && i != 8 && (sync & 2)));
                f.set_value_f01(params[i]);
            }
            if (v.previous_high && !v.high)
                v.kernel.release();
            v.previous_high = v.high;
            if (counts[3])
                s.start_phase.set_value_f01((c < counts[3] ? inputs[3][c] : 0) * .1);
            for (unsigned i = 0; i < 13; ++i)
                v.values[i] = field(s, i).val;
            std::array<float, 10> normalized{};
            for (unsigned i = 0; i < 10; ++i)
            {
                float cv = counts[4 + i] == 1  ? inputs[4 + i][0]
                           : c < counts[4 + i] ? inputs[4 + i][c]
                                               : 0;
                float modulation = cv * .2f;
                normalized[i] = params[i] + modulation;
                v.values[offsets[i]].f += modulation * lfo_range[i];
            }
            float amp[3]{1, params[51] > .5f ? normalized[3] : 1,
                         params[51] > .5f ? normalized[3] : 1};
            v.kernel.onepoleFactor = params[57];
            bool attack = v.attack, env_attack = v.env_attack;
            v.attack = v.env_attack = false;
            auto retrigger_mode =
                params[56] > .5f ? lfo::LFOKernel::FROM_ZERO : lfo::LFOKernel::FROM_LAST;
            if (attack)
            {
                v.kernel.envRetrigMode = retrigger_mode;
                v.kernel.attackFrom(params[56] > .5f ? 0 : v.kernel.get_output(2));
                v.prior_phase = v.prior_env = -1;
            }
            v.kernel.process_block();
            if (attack)
            {
                for (unsigned i = 0; i < 3; ++i)
                    v.previous[i] = v.kernel.get_output(i) * amp[i];
                v.kernel.process_block();
            }
            else if (env_attack)
            {
                v.kernel.envRetrigMode = retrigger_mode;
                v.kernel.retriggerEnvelopeFrom(params[56] > .5f ? 0 : v.kernel.get_output(2));
            }
            if (v.kernel.getIntPhase() != v.prior_phase)
            {
                v.prior_phase = v.kernel.getIntPhase();
                v.countdown[0] = p->pulse_samples;
            }
            bool new_stage = v.kernel.getEnvState() != v.prior_env;
            v.prior_env = v.kernel.getEnvState();
            bool ended = new_stage && v.prior_env == lfo::lfoeg_stuck && !v.high;
            for (unsigned i = 0; i < 2; ++i)
                if (!v.countdown[i + 1])
                {
                    bool pulse = trigger_type[i] == 1   ? ended
                                 : trigger_type[i] == 2 ? new_stage
                                 : trigger_type[i] == 3 ? v.kernel.retrigger_FEG
                                                        : v.kernel.retrigger_AEG;
                    if (pulse)
                        v.countdown[i + 1] = p->pulse_samples;
                }
            v.kernel.retrigger_FEG = v.kernel.retrigger_AEG = false;
            for (unsigned i = 0; i < 3; ++i)
                v.next[i] = v.kernel.get_output(i) * amp[i];
            if (!gate_connected && params[54] < .5f)
                v.next[2] = 0;
        }
        if (rack_compat)
            for (unsigned c = channels; c < 4; ++c)
                p->voices[c].next = {};
    }
    float frac = float(p->position) / 8,
          mul = (rack_compat ? p->voices[0].settings.unipolar.val.b : params[11] > .5f) ? 10 : 5;
    for (unsigned c = 0; c < 4; ++c)
    {
        if (c >= channels)
        {
            for (auto &o : out)
                o[c] = 0;
            continue;
        }
        auto &v = p->voices[c];
        for (unsigned i = 0; i < 3; ++i)
            out[i][c] = (v.previous[i] * (1.f - frac) + v.next[i] * frac) * mul;
        for (unsigned i = 0; i < 3; ++i)
        {
            out[3 + i][c] = v.countdown[i] ? 10 : 0;
            if (v.countdown[i])
                --v.countdown[i];
        }
    }
    p->position = (p->position + 1) % 8;
}
} // namespace surge_mm
