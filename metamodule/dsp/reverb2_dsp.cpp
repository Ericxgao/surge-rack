// SPDX-License-Identifier: GPL-3.0-or-later
#include "reverb2_dsp.h"
#include "Reverb2Kernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    reverb2::Reverb2Storage global;
    reverb2::Reverb2Settings settings;
    std::array<reverb2::Reverb2Value, 10> values{};
    reverb2::Reverb2Kernel<> kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    void reset()
    {
        pending = true;
        for (auto &v : input)
            std::fill_n(v, 8, 0);
        for (auto &v : processed)
            std::fill_n(v, 8, 0);
    }
};
} // namespace
struct Reverb2
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    TempoClock tempo;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    Reverb2()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Reverb2 *create_reverb2()
{
    return new Reverb2;
}
void destroy_reverb2(Reverb2 *p)
{
    delete p;
}
void reverb2_rack_mode(Reverb2 *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        auto fresh = [sr](std::unique_ptr<Voice> &v) {
            v = std::make_unique<Voice>();
            v->global.set_samplerate(sr);
            v->kernel.expand_rack_storage();
        };
        fresh(p->rack_mono);
        p->rack_mono->kernel.init();
        p->rack_mono->kernel.init_default_values();
        for (auto &v : p->voices)
            fresh(v);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
        p->tempo = {};
    }
}
void reset_reverb2(Reverb2 *p)
{
    p->position = 0;
    p->tempo = {};
    for (auto &v : p->voices)
        v->reset();
}
void reverb2_samplerate(Reverb2 *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_reverb2(p);
}
void process_reverb2(Reverb2 *p, const std::array<float, 12> &params,
                     const std::array<PolyValues, 10> &cv, const StereoPoly &input,
                     unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[10] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_reverb2(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                p->voices[c]->kernel.init();
                p->rack_created[c] = true;
            }
        p->rack_mono->kernel.init();
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                v.kernel.init();
            for (auto &b : v.input)
                std::fill_n(b, 8, 0);
            for (auto &b : v.processed)
                std::fill_n(b, 8, 0);
        }
        p->position = 0;
    }
    p->tempo.process(clock, connected, params[11] > .5f, p->voices[0]->global.samplerate);
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 10; ++i)
                v.values[i].f = imported ? (reverb2_minimum[i] + params[i] * reverb2_range[i]) +
                                               cv[i][c] * .2f * reverb2_range[i]
                                         : reverb2_minimum[i] +
                                               std::clamp(params[i] + cv[i][c] * .2f, 0.f, 1.f) *
                                                   reverb2_range[i];
            v.settings.p[0].temposync = p->tempo.synced;
            v.global.temposyncratio_inv = 1 / p->tempo.ratio;
            if (p->tempo.synced)
            {
                float value = round_tempo_time(-8 + params[0] * 9) + cv[0][c] * 1.8f;
                v.values[0].f = imported ? value : std::clamp(value, -8.f, 1.f);
            }
            if (!imported && v.pending)
            {
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
