// SPDX-License-Identifier: GPL-3.0-or-later
#include "neuron_dsp.h"
#include "fx_clock.h"
#include "NeuronKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    neuron::NeuronStorage global;
    neuron::NeuronSettings settings;
    std::array<neuron::NeuronValue, 12> values{};
    neuron::NeuronKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    explicit Voice(bool imported = false, float sr = 48000)
    {
        settings.p[8].val_min.f = -7;
        settings.p[8].val_max.f = 9;
        global.set_samplerate(sr);
        kernel.modLFO.setSampleRate(sr);
        if (imported)
            kernel.init();
        else
            kernel.reset_native();
    }
    void reset()
    {
        kernel.reset_native();
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Neuron
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    neuron::NeuronSettings rack_settings;
    std::array<neuron::NeuronValue, 12> rack_values{};
    std::array<bool, 4> rack_created{};
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    Neuron()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Neuron *create_neuron()
{
    return new Neuron;
}
void destroy_neuron(Neuron *p)
{
    delete p;
}
void neuron_rack_init(Neuron *p, Voice &v)
{
    v.settings = p->rack_settings;
    v.values = p->rack_values;
    v.global.temposyncratio = p->tempo.ratio;
    v.kernel.init();
}
void neuron_rack_clock(Neuron *p, bool enabled)
{
    p->tempo.set_rack_mode(enabled);
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(true, sr);
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(true, sr);
        p->rack_settings = p->rack_mono->settings;
        p->rack_values = {};
        p->rack_mono->kernel.init_default_values();
        p->rack_settings = p->rack_mono->settings;
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_neuron(Neuron *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void neuron_samplerate(Neuron *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_neuron(p);
}
void process_neuron(Neuron *p, const std::array<float, 15> &params,
                    const std::array<PolyValues, 12> &cv, const StereoPoly &input,
                    unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[13] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_neuron(p);
        p->channels = channels;
    }
    p->tempo.process(clock, connected, params[14] > .5f, p->voices[0]->global.samplerate);
    if (imported)
    {
        p->rack_settings.p[8].temposync = p->tempo.synced;
        if (poly && channels != p->last_rack_channels)
        {
            p->last_rack_channels = channels;
            for (unsigned c = 0; c < channels; ++c)
                if (!p->rack_created[c])
                {
                    auto &v = *p->voices[c];
                    v.kernel.modLFO.setSampleRate(v.global.samplerate);
                    neuron_rack_init(p, v);
                    p->rack_created[c] = true;
                }
            neuron_rack_init(p, *p->rack_mono);
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &v = *p->voices[c];
                if (p->rack_created[c])
                    neuron_rack_init(p, v);
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
            for (unsigned i = 0; i < 12; ++i)
                p->rack_settings.p[i].val.f = neuron_minimum[i] + params[i] * neuron_range[i];
            p->rack_settings.p[7].val.i = int(std::round(p->rack_settings.p[7].val.f));
            if (p->tempo.synced)
                p->rack_settings.p[8].val.f = round_tempo_time(p->rack_settings.p[8].val.f);
            p->rack_settings.p[8].deactivated = params[12] < .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 12; ++i)
            {
                const auto normalized = params[i] + cv[i][c] * neuron_cv_scale(i);
                v.values[i].f =
                    neuron_minimum[i] + std::clamp(normalized, 0.f, 1.f) * neuron_range[i];
                if (i == 8)
                    v.values[i].f = std::clamp(-7 + normalized * 16, -8.f, 10.f);
                if (i == 9)
                    v.values[i].f = std::clamp(normalized, 0.f, 2.f);
            }
            v.values[7].i = int(std::round(v.values[7].f));
            v.settings.p[8].deactivated = params[12] < .5f;
            v.settings.p[8].temposync = p->tempo.synced;
            v.global.temposyncratio = p->tempo.ratio;
            if (p->tempo.synced && params[12] > .5f)
                v.values[8].f = std::clamp(
                    round_tempo_time(-7 + params[8] * 16) + cv[8][c] * 16 / 5, -8.f, 10.f);
            if (imported)
            {
                v.settings = p->rack_settings;
                for (unsigned i = 0; i < 12; ++i)
                    if (i != 7)
                        v.values[i].f = p->rack_settings.p[i].val.f +
                                        cv[i][c] * neuron_cv_scale(i) * neuron_range[i];
                p->rack_values = v.values;
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
