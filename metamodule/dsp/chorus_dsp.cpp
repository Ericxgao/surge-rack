// SPDX-License-Identifier: GPL-3.0-or-later
#include "chorus_dsp.h"
#include "fx_clock.h"
#include "ChorusKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    chorus::ChorusStorage global;
    chorus::ChorusSettings settings;
    std::array<chorus::ChorusValue, 8> values{};
    chorus::ChorusKernel<4> kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    void fresh_kernel()
    {
        kernel.~ChorusKernel();
        settings = {};
        values = {};
        new (&kernel) chorus::ChorusKernel<4>(&global, &settings, values.data());
    }
    void reset()
    {
        pending = true;
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Chorus
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    chorus::ChorusSettings rack_settings;
    std::array<bool, 4> rack_created{};
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    Chorus()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Chorus *create_chorus()
{
    return new Chorus;
}
void destroy_chorus(Chorus *p)
{
    delete p;
}
void chorus_rack_clock(Chorus *p, bool enabled)
{
    p->tempo.set_rack_mode(enabled);
    if (enabled)
    {
        if (!p->rack_mono)
            p->rack_mono = std::make_unique<Voice>();
        p->rack_mono->global.set_samplerate(p->voices[0]->global.samplerate);
        p->rack_mono->fresh_kernel();
        p->rack_mono->kernel.init();
        p->rack_mono->kernel.init_default_values();
        p->rack_mono->pending = false;
        p->rack_settings = p->rack_mono->settings;
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
        for (auto &v : p->voices)
        {
            v->fresh_kernel();
            v->reset();
        }
    }
}
void reset_chorus(Chorus *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void chorus_samplerate(Chorus *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_chorus(p);
}
void process_chorus(Chorus *p, const std::array<float, 12> &params,
                    const std::array<PolyValues, 8> &cv, const StereoPoly &input, unsigned channels,
                    float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[chorus_param::polyphony] > .5f;
    if (imported)
    {
        p->tempo.process(clock, connected, params[chorus_param::clock_format] > .5f,
                         p->voices[0]->global.samplerate);
        if (poly && channels != p->last_rack_channels)
        {
            p->last_rack_channels = channels;
            for (unsigned c = 0; c < channels; ++c)
                if (!p->rack_created[c])
                {
                    auto &v = *p->voices[c];
                    v.settings = p->rack_settings;
                    v.kernel.init();
                    p->rack_created[c] = true;
                }
            p->rack_mono->settings = p->rack_settings;
            p->rack_mono->kernel.init();
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &v = *p->voices[c];
                if (p->rack_created[c])
                {
                    v.settings = p->rack_settings;
                    v.kernel.init();
                    v.pending = false;
                }
                v.reset();
            }
            p->position = 0;
        }
    }
    else
    {
        if (channels != p->channels)
        {
            reset_chorus(p);
            p->channels = channels;
        }
        // Native resets historically happen before sampling the clock.
        p->tempo.process(clock, connected, params[chorus_param::clock_format] > .5f,
                         p->voices[0]->global.samplerate);
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
                p->rack_settings.p[i].val.f = chorus_minimum[i] + params[i] * chorus_range[i];
            p->rack_settings.p[1].temposync = p->tempo.synced;
            if (p->tempo.synced)
                p->rack_settings.p[1].val.f = round_tempo_time(p->rack_settings.p[1].val.f);
            p->rack_settings.p[4].deactivated = params[chorus_param::low_cut_on] < .5f;
            p->rack_settings.p[5].deactivated = params[chorus_param::high_cut_on] < .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 8; ++i)
            {
                float scale = i == 4 || i == 5 ? 12.f / 130 : .2f;
                v.values[i].f =
                    chorus_minimum[i] +
                    std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) * chorus_range[i];
            }
            // ct_chorusmodtime is not tempo-syncable in Rack. Only Rate follows Clock.
            v.settings.p[1].temposync = p->tempo.synced;
            v.global.temposyncratio = p->tempo.ratio;
            v.global.temposyncratio_inv = p->tempo.inverse;
            if (p->tempo.synced)
                v.values[1].f = std::clamp(
                    round_tempo_time(-7 + params[1] * 16) + cv[1][c] / 5 * 16, -7.f, 9.f);
            v.settings.p[4].deactivated = params[chorus_param::low_cut_on] < .5f;
            v.settings.p[5].deactivated = params[chorus_param::high_cut_on] < .5f;
            if (imported)
            {
                v.settings = p->rack_settings;
                v.pending = false;
                // Rack bounds/snaps the base Parameter, then adds modulation
                // in globaldata. The engine owns any resulting output bounds.
                for (unsigned i = 0; i < 8; ++i)
                {
                    float scale = i == 4 || i == 5 ? 12.f / 130 : .2f;
                    v.values[i].f =
                        p->rack_settings.p[i].val.f + cv[i][c] * scale * chorus_range[i];
                }
            }
            else if (v.pending)
            {
                for (unsigned i = 0; i < 8; ++i)
                    v.settings.p[i].val = v.values[i];
                v.kernel.reset_native();
                v.pending = false;
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
