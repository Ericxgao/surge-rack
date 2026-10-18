// SPDX-License-Identifier: GPL-3.0-or-later
#include "frequency_shifter_dsp.h"
#include "fx_clock.h"
#include "FrequencyShifterKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    frequency_shifter::FrequencyShifterStorage global;
    frequency_shifter::FrequencyShifterSettings settings;
    std::array<frequency_shifter::FrequencyShifterValue, 5> values{};
    frequency_shifter::FrequencyShifterKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
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
struct FrequencyShifter
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    frequency_shifter::FrequencyShifterSettings rack_settings;
    std::array<frequency_shifter::FrequencyShifterValue, 5> rack_values{};
    std::array<bool, 4> rack_created{};
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    FrequencyShifter()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
FrequencyShifter *create_frequency_shifter()
{
    return new FrequencyShifter;
}
void destroy_frequency_shifter(FrequencyShifter *p)
{
    delete p;
}
void frequency_shifter_rack_init(FrequencyShifter *p, Voice &v)
{
    v.settings = p->rack_settings;
    v.values = p->rack_values;
    v.global.temposyncratio_inv = p->tempo.inverse;
    v.kernel.init();
    v.pending = false;
}
void frequency_shifter_rack_clock(FrequencyShifter *p, bool enabled)
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
        frequency_shifter_rack_init(p, *p->rack_mono);
        p->rack_mono->kernel.init_default_values();
        p->rack_settings = p->rack_mono->settings;
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_frequency_shifter(FrequencyShifter *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void frequency_shifter_samplerate(FrequencyShifter *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_frequency_shifter(p);
}
void process_frequency_shifter(FrequencyShifter *p, const std::array<float, 8> &params,
                               const std::array<PolyValues, 5> &cv, const StereoPoly &input,
                               unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[6] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_frequency_shifter(p);
        p->channels = channels;
    }
    p->tempo.process(clock, connected, params[7] > .5f, p->voices[0]->global.samplerate);
    if (imported)
    {
        p->rack_settings.p[2].temposync = p->tempo.synced;
        if (poly && channels != p->last_rack_channels)
        {
            p->last_rack_channels = channels;
            for (unsigned c = 0; c < channels; ++c)
                if (!p->rack_created[c])
                {
                    frequency_shifter_rack_init(p, *p->voices[c]);
                    p->rack_created[c] = true;
                }
            frequency_shifter_rack_init(p, *p->rack_mono);
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &v = *p->voices[c];
                if (p->rack_created[c])
                    frequency_shifter_rack_init(p, v);
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
            for (unsigned i = 0; i < 5; ++i)
                p->rack_settings.p[i].val.f =
                    frequency_shifter_minimum[i] + params[i] * frequency_shifter_range[i];
            if (p->tempo.synced)
                p->rack_settings.p[2].val.f = round_tempo_time(p->rack_settings.p[2].val.f);
            p->rack_settings.p[0].extend_range = params[5] > .5f;
        }
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 5; ++i)
                v.values[i].f =
                    frequency_shifter_minimum[i] +
                    std::clamp(params[i] + cv[i][c] * .2f, 0.f, 1.f) * frequency_shifter_range[i];
            v.settings.p[0].extend_range = params[5] > .5f;
            v.settings.p[2].temposync = p->tempo.synced;
            v.global.temposyncratio_inv = p->tempo.inverse;
            if (p->tempo.synced)
                v.values[2].f =
                    std::clamp(round_tempo_time(-8 + params[2] * 8) + cv[2][c] * 1.6f, -8.f, 0.f);
            if (imported)
            {
                v.settings = p->rack_settings;
                for (unsigned i = 0; i < 5; ++i)
                    v.values[i].f =
                        p->rack_settings.p[i].val.f + cv[i][c] * .2f * frequency_shifter_range[i];
                p->rack_values = v.values;
            }
            else
            {
                // Native initialization uses the first block's CV-adjusted time.
                for (unsigned i = 0; i < 5; ++i)
                    v.settings.p[i].val = v.values[i];
                if (v.pending)
                {
                    v.kernel.reset_native();
                    v.pending = false;
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
