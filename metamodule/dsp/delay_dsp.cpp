// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from src/Delay.h, Surge Synth Team 2019-2024.
#include "delay_dsp.h"
#include "filter_tuning.h"
#include "modulation_clock.h"
#include "rack_clock.h"
#include <cstring>
#include <sst/utilities/globals.h>
#include <sst/basic-blocks/dsp/SSESincDelayLine.h>
#include <sst/filters/BiquadFilter.h>
namespace surge_mm
{
namespace
{
struct Storage
{
    float samplerate{48000}, samplerate_inv{1.f / 48000};
    double dsamplerate_inv{1.0 / 48000};
    static float note_to_pitch_ignoring_tuning(float n)
    {
        return FilterTuning::note_to_pitch_ignoring_tuning(n);
    }
};
using Line = sst::basic_blocks::dsp::SSESincDelayLine<1 << 19>;
using Biquad = sst::filters::Biquad::BiquadFilter<Storage, 8>;
const auto &sinc()
{
    static const sst::basic_blocks::tables::SurgeSincTableProvider table;
    return table;
}
// Keep the original float modf/pow and double log2 evaluation.
float round_time(float f)
{
    float a, b = std::modf(f, &a);
    if (b < 0)
    {
        b += 1.f;
        a -= 1.f;
    }
    b = std::pow(2.f, b);
    if (b > 1.41f)
        b = std::log2(double(1.5f));
    else if (b > 1.167f)
        b = std::log2(double(1.3333333333f));
    else
        b = 0;
    return a + b;
}
} // namespace
struct Delay
{
    Storage storage;
    Line left{sinc()}, right{sinc()};
    Biquad lp{&storage}, hp{&storage};
    TempoClock tempo;
    RackClock rack_clock;
    bool rack_compat{};
    std::array<float, 10> previous{};
    unsigned position{}, clip{3};
    float ts_l{}, ts_r{}, mod_value{}, mod_delta{}, mod_phase{};
    std::array<float, 2> times{};
    Delay()
    {
        (void)FilterTuning::note_to_pitch(0);
        lp.suspend();
        hp.suspend();
    }
};
Delay *create_delay()
{
    return new Delay;
}
void destroy_delay(Delay *p)
{
    delete p;
}
void delay_rack_mode(Delay *p, bool enabled)
{
    p->rack_compat = enabled;
    if (enabled)
    {
        p->rack_clock.ratio = 1.f / 120;
        p->rack_clock.inverse = 1 / p->rack_clock.ratio;
    }
}
void reset_delay(Delay *p)
{
    p->left.clear();
    p->right.clear();
    // suspend clears histories but not coefficients: restore constructor state too.
    p->lp = Biquad(&p->storage);
    p->hp = Biquad(&p->storage);
    p->lp.suspend();
    p->hp.suspend();
    p->tempo = {};
    p->rack_clock = {};
    p->previous = {};
    p->position = 0;
    p->clip = 3;
    p->ts_l = p->ts_r = p->mod_value = p->mod_delta = p->mod_phase = 0;
    p->times = {};
}
void delay_samplerate(Delay *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    p->storage.samplerate = sr;
    p->storage.samplerate_inv = 1.f / sr;
    p->storage.dsamplerate_inv = 1.0 / sr;
    if (p->rack_compat)
        delay_rack_mode(p, true);
    else
        reset_delay(p);
}
std::array<float, 2> delay_times(const Delay *p)
{
    return p->times;
}
void process_delay(Delay *p, const std::array<float, 12> &params, const std::array<float, 10> &cv,
                   const std::array<float, 2> &audio, float clock, bool connected,
                   std::array<float, 2> &out, bool rack_compat)
{
    bool synced;
    float inverse;
    if (rack_compat)
    {
        p->rack_clock.process(clock, connected, params[11] > .5f, p->storage.samplerate);
        synced = p->rack_clock.synced;
        inverse = p->rack_clock.inverse;
    }
    else
    {
        p->tempo.process(clock, connected, params[11] > .5f, p->storage.samplerate);
        synced = p->tempo.synced;
        inverse = 1 / p->tempo.ratio;
    }
    if (p->position == 8)
    {
        p->position = 0;
        p->clip = unsigned(std::round(params[10] * 3));
        // Rack evaluates these using the previous sample's modulation values.
        p->lp.coeff_LP2B(p->lp.calc_omega(p->previous[6] / 12.0), .707);
        p->hp.coeff_HP(p->lp.calc_omega(p->previous[5] / 12.0), .707);
        float rate = std::clamp(p->previous[7], 0.f, 4.f);
        rate = rate * rate;
        auto phase_delta = 8 * p->storage.samplerate_inv * rate;
        p->mod_phase += phase_delta;
        if (p->mod_phase > 1)
            p->mod_phase -= 1;
        float target = std::sin(p->mod_phase * 2.0 * 3.14159265358979323846);
        p->mod_delta = (target - p->mod_value) / 8;
        if (synced)
        {
            p->ts_l = round_time(delay_time_min + params[0] * delay_time_range);
            p->ts_r = round_time(delay_time_min + params[1] * delay_time_range);
        }
    }
    std::array<float, 10> values{}, mod{};
    for (unsigned i = 0; i < 10; ++i)
    {
        mod[i] = delay_range[i] * (cv[i] * .2f);
        values[i] = (delay_minimum[i] + params[i] * delay_range[i]) + mod[i];
    }
    float il = audio[0] * .2f, ir = audio[1] * .2f;
    p->mod_value += p->mod_delta;
    auto wobble = 1.0 + .02 * values[2] + .005 * values[8] * p->mod_value;
    float tl{}, tr{};
    if (synced)
    {
        auto tvl = 12 * wobble * (p->ts_l + mod[0]), tvr = 12 * wobble * (p->ts_r + mod[1]);
        tl = p->storage.samplerate * inverse * Storage::note_to_pitch_ignoring_tuning(tvl);
        tr = p->storage.samplerate * inverse * Storage::note_to_pitch_ignoring_tuning(tvr);
    }
    else
    {
        tl =
            p->storage.samplerate * Storage::note_to_pitch_ignoring_tuning(12 * wobble * values[0]);
        tr =
            p->storage.samplerate * Storage::note_to_pitch_ignoring_tuning(12 * wobble * values[1]);
    }
    tl = std::clamp(tl, 0.f, float(1 << 19));
    tr = std::clamp(tr, 0.f, float(1 << 19));
    p->times = {tl * p->storage.samplerate_inv, tr * p->storage.samplerate_inv};
    auto dl = p->left.read(tl), dr = p->right.read(tr);
    switch (p->clip)
    {
    case 1:
        dl = std::clamp(dl, -1.5f, 1.5f);
        dr = std::clamp(dr, -1.5f, 1.5f);
        dl = dl - 4.f / 27.f * dl * dl * dl;
        dr = dr - 4.f / 27.f * dr * dr * dr;
        break;
    case 2:
        dl = std::clamp(dl, -3.f, 3.f);
        dr = std::clamp(dr, -3.f, 3.f);
        dl = dl * .5;
        dr = dr * .5;
        dl = 2.f * dl - 8.f / 27.f * dl * dl * dl;
        dr = 2.f * dr - 8.f / 27.f * dr * dr * dr;
        break;
    case 3:
        dl = std::clamp(dl, -2.f, 2.f);
        dr = std::clamp(dr, -2.f, 2.f);
        break;
    default:
        break;
    }
    float wl = il + values[3] * dl + values[4] * dr, wr = ir + values[3] * dr + values[4] * dl;
    p->lp.process_sample(wl, wr, wl, wr);
    p->hp.process_sample(wl, wr, wl, wr);
    p->left.write(wl);
    p->right.write(wr);
    auto mix = values[9];
    out = {(mix * dl + (1 - mix) * il) * 5, (mix * dr + (1 - mix) * ir) * 5};
    p->previous = values;
    ++p->position;
}
} // namespace surge_mm
