// SPDX-License-Identifier: GPL-3.0-or-later
#include "ensemble_dsp.h"
#include "EnsembleKernel.h"
namespace surge_mm
{
namespace
{
struct Voice
{
    ensemble::EnsembleStorage global;
    ensemble::EnsembleSettings settings;
    std::array<ensemble::EnsembleValue, 12> values{};
    ensemble::EnsembleKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    static ensemble::EnsembleStorage storage_at_rate(float sr)
    {
        ensemble::EnsembleStorage s;
        s.set_samplerate(sr);
        return s;
    }
    Voice(float sr = 48000) : global(storage_at_rate(sr))
    {
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
struct Ensemble
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    ensemble::EnsembleSettings rack_settings;
    std::array<ensemble::EnsembleValue, 12> rack_values{};
    std::array<bool, 4> rack_created{};
    bool rack_mode{};
    unsigned position{}, channels{1}, last_rack_channels{};
    Ensemble()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Ensemble *create_ensemble()
{
    return new Ensemble;
}
void destroy_ensemble(Ensemble *p)
{
    delete p;
}
void ensemble_rack_init(Ensemble *p, Voice &v)
{
    v.settings = p->rack_settings;
    v.values = p->rack_values;
    v.kernel.init();
    v.pending = false;
}
void ensemble_rack_mode(Ensemble *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(sr);
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(sr);
        p->rack_settings = {};
        p->rack_values = {};
        ensemble_rack_init(p, *p->rack_mono);
        p->rack_mono->kernel.init_default_values();
        p->rack_settings = p->rack_mono->settings;
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_ensemble(Ensemble *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void ensemble_samplerate(Ensemble *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_ensemble(p);
}
void process_ensemble(Ensemble *p, const std::array<float, 14> &params,
                      const std::array<PolyValues, 12> &cv, const StereoPoly &input,
                      unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[13] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_ensemble(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                // Rack constructs this engine at its first connection, possibly at
                // a different rate from the module's initial load. Reuse storage.
                p->voices[c]->kernel.sampleRateReset();
                ensemble_rack_init(p, *p->voices[c]);
                p->rack_created[c] = true;
            }
        ensemble_rack_init(p, *p->rack_mono);
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                ensemble_rack_init(p, v);
            for (auto &b : v.input)
                std::fill_n(b, 8, 0);
            for (auto &b : v.processed)
                std::fill_n(b, 8, 0);
        }
        p->position = 0;
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
                p->rack_settings.p[i].val.f = ensemble_minimum[i] + params[i] * ensemble_range[i];
            p->rack_settings.p[5].val.i = int(std::round(p->rack_settings.p[5].val.f));
            p->rack_settings.p[11].deactivated = params[12] < .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 12; ++i)
            {
                float scale = i == 0 || i == 11 ? 12.f / 130 : .2f;
                v.values[i].f =
                    ensemble_minimum[i] +
                    std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) * ensemble_range[i];
            }
            // The original engine explicitly allows modulation one octave beyond
            // the two LFO knobs (0.005..40 Hz instead of 0.01..20 Hz).
            for (unsigned i : {1u, 3u})
                v.values[i].f = std::clamp(ensemble_minimum[i] +
                                               (params[i] + cv[i][c] * .2f) * ensemble_range[i],
                                           -7.64386f, 5.322f);
            v.values[5].i = int(std::round(v.values[5].f));
            v.settings.p[11].deactivated = params[12] < .5f;
            if (imported)
            {
                v.settings = p->rack_settings;
                for (unsigned i = 0; i < 12; ++i)
                    if (i != 5)
                        v.values[i].f =
                            p->rack_settings.p[i].val.f +
                            cv[i][c] * (i == 0 || i == 11 ? 12.f / 130 : .2f) * ensemble_range[i];
                p->rack_values = v.values;
            }
            else if (v.pending)
            {
                for (unsigned i = 0; i < 12; ++i)
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
