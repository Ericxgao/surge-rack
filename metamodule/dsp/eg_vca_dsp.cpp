// SPDX-License-Identifier: GPL-3.0-or-later
// Native adaptation of src/EGxVCA.h, copyright Surge Synth Team contributors.
#include "eg_vca_dsp.h"
#include "modulation_clock.h"
#include <cstring>
#include <memory>
#include "sst/basic-blocks/modulators/ADSREnvelope.h"
#include "sst/basic-blocks/modulators/DAHDEnvelope.h"
#include "sst/basic-blocks/dsp/PanLaws.h"
namespace surge_mm
{
namespace mod = sst::basic_blocks::modulators;
struct EGxVCA
{
    using ADSR = mod::ADSREnvelope<EnvelopeClock, 8>;
    using SlowADSR = mod::ADSREnvelope<EnvelopeClock, 8, mod::TwoMinuteRange>;
    using DAHD = mod::DAHDEnvelope<EnvelopeClock, 8>;
    using SlowDAHD = mod::DAHDEnvelope<EnvelopeClock, 8, mod::TwoMinuteRange>;
    EnvelopeClock clock;
    TempoClock tempo;
    std::array<std::unique_ptr<ADSR>, 4> adsr;
    std::array<std::unique_ptr<SlowADSR>, 4> slow_adsr;
    std::array<std::unique_ptr<DAHD>, 4> dahd;
    std::array<std::unique_ptr<SlowDAHD>, 4> slow_dahd;
    Edge triggers[4]{};
    unsigned rack_gates[4]{2, 2, 2, 2};
    struct RackTempo
    {
        unsigned gate{2};
        int elapsed{-1};
        float ratio{1}, last_bpm{-1}, last_volts{-11};
        bool connected{}, synced{};
        static bool edge(unsigned &state, float v)
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
                    ratio = float(std::pow(2.0, volts));
                last_volts = volts;
            }
            else
            {
                if (edge(gate, volts))
                {
                    if (elapsed > 0 && elapsed < sr * 6)
                    {
                        float rate = 60 * sr / elapsed;
                        if (std::abs(rate - std::round(rate)) < .015f)
                            rate = std::round(rate);
                        if (rate != last_bpm)
                            ratio = rate / 120;
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
    } rack_tempo;
    struct Interpolate
    {
        float value{}, delta{};
        void target(float next)
        {
            delta = (next - value) / 8;
        }
        void step()
        {
            value += delta;
        }
    } level[4], response[4], pan[4][4];
    std::array<PolyValues, 7> controls{};
    unsigned channels{1}, position{};
    unsigned event_samples[4]{};
    int last_mode{}, last_slow{};
    bool rack_compat{}, rack_started{};
    EGxVCA()
    {
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            adsr[lane] = std::make_unique<ADSR>(&clock);
            slow_adsr[lane] = std::make_unique<SlowADSR>(&clock);
            dahd[lane] = std::make_unique<DAHD>(&clock);
            slow_dahd[lane] = std::make_unique<SlowDAHD>(&clock);
            silence(lane);
            level[lane].value = 1;
            pan[lane][0].value = pan[lane][1].value = 1;
        }
    }
    void silence(unsigned lane)
    {
        adsr[lane]->immediatelySilence();
        slow_adsr[lane]->immediatelySilence();
        dahd[lane]->immediatelySilence();
        slow_dahd[lane]->immediatelySilence();
        event_samples[lane] = 0;
    }
    template <class Envelope>
    void render(const std::array<std::unique_ptr<Envelope>, 4> &engines,
                const std::array<float, 17> &p, const std::array<PolyValues, 11> &in,
                std::array<PolyValues, 4> &out)
    {
        int as = int(std::round(p[8] * 2)), ds = int(std::round(p[9] * 2)),
            rs = int(std::round(p[10] * 2));
        int selected = int(std::round(p[15] * 7));
        for (unsigned lane = 0; lane < channels; ++lane)
        {
            auto &env = *engines[lane];
            bool event = false;
            if (rack_compat ? RackTempo::edge(rack_gates[lane], in[2][lane])
                            : triggers[lane].process(in[2][lane]))
            {
                env.attackFrom((rack_compat ? p[12] >= .5f : p[12] > .5f) ? env.output : 0,
                               controls[3][lane], as, p[7] < .5f);
                event =
                    selected == 1 || selected == 7 ||
                    (!rack_compat && selected == 2 && last_mode && env.stage == Envelope::s_attack);
            }
            auto before = env.stage;
            env.process(controls[3][lane], controls[4][lane], controls[5][lane], controls[6][lane],
                        as, ds, rs, in[2][lane] > 2);
            auto after = env.stage;
            if (before != after)
            {
                event |=
                    selected == 7 ||
                    (selected == 2 && after == Envelope::s_attack && (rack_compat || last_mode)) ||
                    (selected == 3 && after == Envelope::s_sustain && last_mode) ||
                    (selected == 4 && after == Envelope::s_decay && (rack_compat || !last_mode)) ||
                    (selected == 5 && after == Envelope::s_sustain && !last_mode) ||
                    (selected == 6 && after == Envelope::s_release) ||
                    (selected == 0 && (rack_compat || before <= Envelope::s_release) &&
                     after > Envelope::s_release);
            }
            if (rack_compat)
            {
                if (event)
                    event_samples[lane] = unsigned(.01 * clock.samplerate / 8);
                if (before == after && event_samples[lane])
                    --event_samples[lane];
            }
            else if (event)
                event_samples[lane] = unsigned(std::round(clock.samplerate * .01f));
            const float linear = env.output, cubic = env.outputCubed;
            float amplitude = ((1 - response[lane].value) * linear + response[lane].value * cubic) *
                              level[lane].value;
            float left = in[0][lane] * amplitude, right = in[1][lane] * amplitude;
            out[0][lane] = left * pan[lane][0].value + right * pan[lane][2].value;
            out[1][lane] = right * pan[lane][1].value + left * pan[lane][3].value;
            out[2][lane] = linear * 10;
            out[3][lane] = event_samples[lane] ? 10 : 0;
            if (!rack_compat && event_samples[lane])
                --event_samples[lane];
            level[lane].step();
            response[lane].step();
            for (auto &coefficient : pan[lane])
                coefficient.step();
        }
    }
};
EGxVCA *create_eg_vca()
{
    return new EGxVCA;
}
void destroy_eg_vca(EGxVCA *engine)
{
    delete engine;
}
void eg_vca_rack_mode(EGxVCA *p, bool enabled)
{
    p->rack_compat = enabled;
    if (enabled)
    {
        p->rack_tempo.ratio = 1.f / 120;
    }
}
void reset_eg_vca(EGxVCA *e)
{
    e->tempo.reset();
    e->rack_tempo = {};
    e->rack_started = false;
    std::fill_n(e->rack_gates, 4, 2);
    e->position = 0;
    e->channels = 1;
    e->last_mode = e->last_slow = 0;
    for (unsigned c = 0; c < 4; ++c)
    {
        e->silence(c);
        e->triggers[c] = {};
        e->level[c] = {1, 0};
        e->response[c] = {};
        for (unsigned q = 0; q < 4; ++q)
            e->pan[c][q] = {q < 2 ? 1.f : 0.f, 0};
    }
}
void set_eg_vca_samplerate(EGxVCA *e, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    e->clock.set_samplerate(sr);
    e->tempo.reset();
    if (e->rack_compat)
        eg_vca_rack_mode(e, true);
    for (unsigned lane = 0; lane < 4; ++lane)
    {
        e->adsr[lane]->onSampleRateChanged();
        e->slow_adsr[lane]->onSampleRateChanged();
        e->dahd[lane]->onSampleRateChanged();
        e->slow_dahd[lane]->onSampleRateChanged();
    }
}
void process_eg_vca(EGxVCA *e, const std::array<float, 17> &p, const std::array<PolyValues, 11> &in,
                    const std::array<unsigned, 4> &counts, std::array<PolyValues, 4> &out,
                    unsigned &channels, bool rack_compat)
{
    e->rack_compat = rack_compat;
    int mode = int(rack_compat ? p[11] >= .5f : p[11] > .5f),
        slow = int(rack_compat ? p[13] >= .5f : p[13] > .5f);
    if (mode != e->last_mode || slow != e->last_slow)
    {
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            auto event = e->event_samples[lane];
            e->silence(lane);
            if (rack_compat)
                e->event_samples[lane] = event;
        }
        e->last_mode = mode;
        e->last_slow = slow;
        if (!rack_compat)
            e->position = 0;
    }
    if (rack_compat)
        e->rack_tempo.process(in[3][0], counts[3] > 0, p[16] > .5f, e->clock.samplerate);
    else
        e->tempo.process(in[3][0], counts[3] > 0, p[16] > .5f, e->clock.samplerate);
    if (!e->position)
    {
        auto previous = e->channels;
        e->channels = std::clamp(std::max({counts[0], counts[1], counts[2]}), 1u, 4u);
        float span = slow ? EGxVCA::SlowADSR::etScale : EGxVCA::ADSR::etScale;
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            if (lane >= e->channels)
            {
                if (!rack_compat)
                {
                    e->silence(lane);
                    e->triggers[lane] = {};
                }
                continue;
            }
            if (rack_compat && e->rack_started && lane >= previous)
                e->rack_gates[lane] = 0;
            for (unsigned id = 0; id < 7; ++id)
            {
                float value = p[id] + in[4 + id][lane] / 5.f;
                if (!rack_compat)
                    value = std::clamp(value, 0.f, 1.f);
                if ((rack_compat ? e->rack_tempo.synced : e->tempo.synced) && id >= 3 &&
                    (mode || id != 5))
                {
                    if (rack_compat)
                    {
                        value = (round_tempo_time(p[id] * span - 8) + 8) / span +
                                (1 - e->rack_tempo.ratio) / span + in[4 + id][lane] / 5.f;
                    }
                    else if (!(mode && id == 3 && value <= .0001f))
                    {
                        float logtime = round_tempo_time(p[id] * span - 8) -
                                        std::log2(e->tempo.ratio) + (value - p[id]) * span;
                        value = (std::clamp(logtime, -15.f, 15.f) + 8) / span;
                    }
                }
                e->controls[id][lane] = value;
            }
            float level = e->controls[0][lane] * 2;
            e->level[lane].target(level * level * level);
            e->response[lane].target(e->controls[2][lane]);
            sst::basic_blocks::dsp::pan_laws::panmatrix_t matrix;
            if (!counts[1])
                sst::basic_blocks::dsp::pan_laws::monoEqualPower(e->controls[1][lane], matrix);
            else if (rack_compat ? p[14] >= .5f : p[14] > .5f)
                sst::basic_blocks::dsp::pan_laws::stereoTruePanning(e->controls[1][lane], matrix);
            else
                sst::basic_blocks::dsp::pan_laws::stereoEqualPower(e->controls[1][lane], matrix);
            for (unsigned coefficient = 0; coefficient < 4; ++coefficient)
                e->pan[lane][coefficient].target(matrix[coefficient]);
        }
    }
    out = {};
    channels = e->channels;
    e->rack_started = true;
    if (mode)
    {
        if (slow)
            e->render(e->slow_dahd, p, in, out);
        else
            e->render(e->dahd, p, in, out);
    }
    else
    {
        if (slow)
            e->render(e->slow_adsr, p, in, out);
        else
            e->render(e->adsr, p, in, out);
    }
    e->position = (e->position + 1) % 8;
}
} // namespace surge_mm
