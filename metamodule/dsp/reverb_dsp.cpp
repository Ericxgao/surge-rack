// SPDX-License-Identifier: GPL-3.0-or-later
#include "reverb_dsp.h"
#include "globals.h"
#include "effect_config.h"
#include <sst/effects/Reverb1.h>
#include <memory>
#include <new>
namespace surge_mm
{
namespace
{
using Kernel = sst::effects::reverb1::Reverb1<EffectConfig>;
struct Voice
{
    EffectGlobal global;
    EffectSettings settings;
    std::array<float, 11> values{};
    Kernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    Voice()
    {
        (void)FilterTuning::note_to_pitch(0);
    }
    void fresh()
    {
        kernel.~Kernel();
        settings = {};
        values = {};
        new (&kernel) Kernel(&global, &settings, values.data());
        reset();
    }
    void reset()
    {
        pending = true;
        for (auto &v : input)
            std::fill_n(v, 8, 0);
        for (auto &v : processed)
            std::fill_n(v, 8, 0);
    }
    void initialize()
    {
        // Reconstruct in the existing allocation, clearing filter smoothing as
        // well as delay history. The original class owns no heap allocations.
        kernel.~Kernel();
        new (&kernel) Kernel(&global, &settings, values.data());
        kernel.initialize();
        pending = false;
    }
};
} // namespace
struct Reverb
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    TempoClock tempo;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<float, 11> rack_base{};
    EffectSettings rack_settings;
    bool rack_mode{};
    unsigned last_rack_channels{};
    Reverb()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Reverb *create_reverb()
{
    return new Reverb;
}
void destroy_reverb(Reverb *p)
{
    delete p;
}
void reverb_rack_mode(Reverb *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        // Rack initializes the Sum engine with ct_none storage, then installs
        // defaults. Poly engines are initialized later with the last base values.
        if (!p->rack_mono)
            p->rack_mono = std::make_unique<Voice>();
        p->rack_mono->global.set_samplerate(p->voices[0]->global.samplerate);
        p->rack_mono->fresh();
        p->rack_mono->kernel.initialize();
        p->rack_base = {-4, 0, .5f, 1, .2f, -24, 0, 0, 72, .5f, 0};
        p->rack_settings = {};
        for (auto &v : p->voices)
            v->fresh();
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
        p->tempo = {};
    }
}
void reset_reverb(Reverb *p)
{
    p->position = 0;
    p->tempo = {};
    for (auto &v : p->voices)
        v->reset();
}
void reverb_samplerate(Reverb *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_reverb(p);
}
void process_reverb(Reverb *p, const std::array<float, 15> &params,
                    const std::array<PolyValues, 11> &cv, const StereoPoly &input,
                    unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[13] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_reverb(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        auto init = [p](Voice &v) {
            v.values = p->rack_base;
            v.settings = p->rack_settings;
            v.kernel.initialize();
        };
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                init(*p->voices[c]);
                p->rack_created[c] = true;
            }
        init(*p->rack_mono);
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                init(v);
            v.reset();
        }
        p->position = 0;
    }
    p->tempo.process(clock, connected, params[14] > .5f, p->voices[0]->global.samplerate);
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        if (imported)
        {
            for (unsigned i = 0; i < 11; ++i)
                p->rack_base[i] = reverb_minimum[i] + params[i] * reverb_range[i];
            p->rack_base[1] = std::round(p->rack_base[1]);
            p->rack_settings.deactivated[5] = params[11] < .5f;
            p->rack_settings.deactivated[8] = params[12] < .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 11; ++i)
            {
                float base = params[i];
                v.values[i] = reverb_minimum[i] +
                              std::clamp(base + cv[i][c] * .2f, 0.f, 1.f) * reverb_range[i];
            }
            v.values[1] = std::round(v.values[1]);
            v.settings.deactivated[5] = params[11] < .5f;
            v.settings.deactivated[8] = params[12] < .5f;
            v.settings.synced[0] = p->tempo.synced;
            v.global.tempo_ratio = p->tempo.ratio;
            // Preserve the pinned Reverb1's forward tempo ratio and ring wrapping.
            if (p->tempo.synced)
                v.values[0] =
                    std::clamp(round_tempo_time(-8 + params[0] * 13) + cv[0][c] * 2.6f, -8.f, 5.f);
            if (imported)
            {
                // Rack limits the base Parameter before adding float modulation.
                // Integer Shape ignores the modulation matrix entirely.
                for (unsigned i = 0; i < 11; ++i)
                    v.values[i] = p->rack_base[i] + (i == 1 ? 0 : cv[i][c] * .2f * reverb_range[i]);
                if (p->tempo.synced)
                    v.values[0] = round_tempo_time(p->rack_base[0]) + cv[0][c] * 2.6f;
            }
            else if (v.pending)
                v.initialize();
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
