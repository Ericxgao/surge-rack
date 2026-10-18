// SPDX-License-Identifier: GPL-3.0-or-later
#include "rotary_dsp.h"
#include "fx_clock.h"
#include "RotaryKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
void initialize_shaper(sst::waveshapers::QuadWaveshaperState &state, int model)
{
    float initial[sst::waveshapers::n_waveshaper_registers]{};
    sst::waveshapers::initializeWaveshaperRegister(rotary::FXWaveShapers[model], initial);
    for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
        state.R[i] = _mm_set1_ps(initial[i]);
    state.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());
}
void warm_shapers()
{
    static const bool ready = []() {
        for (int i = 0; i < rotary::n_fxws; ++i)
        {
            sst::waveshapers::QuadWaveshaperState state{};
            initialize_shaper(state, i);
            auto fn = sst::waveshapers::GetQuadWaveshaper(rotary::FXWaveShapers[i]);
            fn(&state, _mm_setzero_ps(), _mm_set1_ps(1));
        }
        return true;
    }();
    (void)ready;
}
struct Kernel : rotary::RotaryKernel
{
    using rotary::RotaryKernel::RotaryKernel;
    void clear()
    {
        nativeRateBounds = true;
        lfo.set_phase(0);
        lf_lfo.set_phase(0);
        dL = {};
        dR = {};
        hornamp[0] = {};
        hornamp[1] = {};
        drive = {};
        width.set_target(0);
        width.instantize();
        mix.set_target(0);
        mix.instantize();
        init();
    }
};
struct Voice
{
    rotary::RotaryStorage global;
    rotary::RotarySettings settings;
    std::array<rotary::RotaryValue, 8> values{};
    Kernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    int model{-1};
    void reset()
    {
        pending = true;
        model = -1;
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Rotary
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    rotary::RotarySettings rack_settings;
    std::array<rotary::RotaryValue, 8> rack_values{};
    std::array<bool, 4> rack_created{};
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    Rotary()
    {
        warm_shapers();
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Rotary *create_rotary()
{
    return new Rotary;
}
void destroy_rotary(Rotary *p)
{
    delete p;
}
void rotary_rack_init(Rotary *p, Voice &v)
{
    v.settings = p->rack_settings;
    v.values = p->rack_values;
    v.global.temposyncratio = p->tempo.ratio;
    v.kernel.nativeRateBounds = false;
    v.kernel.init();
    v.pending = false;
}
void rotary_rack_clock(Rotary *p, bool enabled)
{
    p->tempo.set_rack_mode(enabled);
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        auto fresh = [sr](std::unique_ptr<Voice> &v) {
            v = std::make_unique<Voice>();
            v->global.set_samplerate(sr);
        };
        fresh(p->rack_mono);
        for (auto &v : p->voices)
            fresh(v);
        p->rack_settings = {};
        p->rack_values = {};
        rotary_rack_init(p, *p->rack_mono);
        p->rack_mono->kernel.init_default_values();
        p->rack_settings = p->rack_mono->settings;
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_rotary(Rotary *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void rotary_samplerate(Rotary *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_rotary(p);
}
void process_rotary(Rotary *p, const std::array<float, 11> &params,
                    const std::array<PolyValues, 8> &cv, const StereoPoly &input, unsigned channels,
                    float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[9] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_rotary(p);
        p->channels = channels;
    }
    p->tempo.process(clock, connected, params[10] > .5f, p->voices[0]->global.samplerate);
    if (imported)
    {
        p->rack_settings.p[0].temposync = p->tempo.synced;
        if (poly && channels != p->last_rack_channels)
        {
            p->last_rack_channels = channels;
            for (unsigned c = 0; c < channels; ++c)
                if (!p->rack_created[c])
                {
                    rotary_rack_init(p, *p->voices[c]);
                    p->rack_created[c] = true;
                }
            rotary_rack_init(p, *p->rack_mono);
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &v = *p->voices[c];
                if (p->rack_created[c])
                    rotary_rack_init(p, v);
                for (auto &b : v.input)
                    std::fill_n(b, 8, 0);
                for (auto &b : v.processed)
                    std::fill_n(b, 8, 0);
            }
            p->position = 0;
        }
    }
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        if (imported)
        {
            for (unsigned i = 0; i < 8; ++i)
                p->rack_settings.p[i].val.f = rotary_minimum[i] + params[i] * rotary_range[i];
            p->rack_settings.p[5].val.i = int(std::round(p->rack_settings.p[5].val.f));
            if (p->tempo.synced)
                p->rack_settings.p[0].val.f = round_tempo_time(p->rack_settings.p[0].val.f);
            p->rack_settings.p[4].deactivated = params[8] < .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 8; ++i)
                v.values[i].f = rotary_minimum[i] +
                                std::clamp(params[i] + cv[i][c] * .2f, 0.f, 1.f) * rotary_range[i];
            v.values[5].i = int(std::round(v.values[5].f));
            v.settings.p[0].temposync = p->tempo.synced;
            v.global.temposyncratio = p->tempo.ratio;
            if (p->tempo.synced)
                v.values[0].f = std::clamp(
                    round_tempo_time(-7 + params[0] * 16) + cv[0][c] / 5 * 16, -7.f, 9.f);
            v.settings.p[4].deactivated = params[8] < .5f;
            if (imported)
            {
                v.settings = p->rack_settings;
                for (unsigned i = 0; i < 8; ++i)
                    if (i != 5)
                        v.values[i].f =
                            p->rack_settings.p[i].val.f + cv[i][c] * .2f * rotary_range[i];
                p->rack_values = v.values;
            }
            else
            {
                if (v.pending)
                {
                    v.kernel.clear();
                    v.pending = false;
                }
                if (v.model != v.values[5].i)
                {
                    v.model = v.values[5].i;
                    initialize_shaper(v.kernel.wsState, v.model);
                }
            }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.process(buffer.processed[0], buffer.processed[1]);
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? p->voices[c]->processed[s][p->position] * 5 : 0;
}
} // namespace surge_mm
