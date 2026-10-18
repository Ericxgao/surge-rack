// SPDX-License-Identifier: GPL-3.0-or-later
#include "vocoder_dsp.h"
#include "VocoderKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    vocoder::VocoderStorage global;
    vocoder::VocoderSettings settings;
    std::array<vocoder::VocoderValue, 12> values{};
    vocoder::VocoderKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    Voice(float sr = 48000)
    {
        global.set_samplerate(sr);
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
struct Vocoder
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<vocoder::VocoderValue, 12> shared_values{};
    alignas(16) float rack_modulator[2][8]{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    Vocoder()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Vocoder *create_vocoder()
{
    return new Vocoder;
}
void destroy_vocoder(Vocoder *p)
{
    delete p;
}
void vocoder_rack_mode(Vocoder *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(sr);
        p->rack_mono->kernel.init();
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(sr);
        p->rack_created = {};
        p->shared_values = {};
        p->last_rack_channels = 0;
        p->position = 0;
        for (auto &b : p->rack_modulator)
            std::fill_n(b, 8, 0);
    }
}
void reset_vocoder(Vocoder *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void vocoder_samplerate(Vocoder *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_vocoder(p);
}
void process_vocoder(Vocoder *p, const std::array<float, 12> &params,
                     const std::array<PolyValues, 9> &cv, const StereoPoly &input,
                     const StereoPoly &modulator, unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[11] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_vocoder(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        auto initialize = [&](Voice &v) {
            v.values = p->shared_values;
            v.kernel.init();
        };
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                initialize(*p->voices[c]);
                p->rack_created[c] = true;
            }
        initialize(*p->rack_mono);
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                initialize(v);
            for (auto &b : v.input)
                std::fill_n(b, 8, 0);
            for (auto &b : v.processed)
                std::fill_n(b, 8, 0);
        }
        p->position = 0;
    }
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
        {
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
            if (!imported)
                p->voices[c]->global.audio_in_nonOS[s][p->position] = modulator[s][c] * .2f;
        }
    // Pinned Rack shares the sum across all Poly voices, without the .2 input
    // rescaling used by Sum mode. Preserve that level difference on import.
    if (imported)
        for (unsigned s = 0; s < 2; ++s)
            p->rack_modulator[s][p->position] = modulator[s][0] * (poly ? 1 : .2f);
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 9; ++i)
            {
                float scale = i == 4 ? 12.f / 72 : i == 5 ? 12.f / 60 : .2f;
                if (imported)
                    v.values[vocoder_engine_ids[i]].f = vocoder_minimum[i] +
                                                        params[i] * vocoder_range[i] +
                                                        cv[i][c] * scale * vocoder_range[i];
                else
                    v.values[vocoder_engine_ids[i]].f =
                        vocoder_minimum[i] +
                        std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) * vocoder_range[i];
            }
            v.values[5].i = 4 + 4 * int(std::round(params[9] * 4));
            v.values[8].i = int(std::round(params[10] * 3));
            if (!imported && v.pending)
            {
                v.kernel.reset_native();
                v.pending = false;
            }
            if (imported)
                for (unsigned s = 0; s < 2; ++s)
                    std::copy_n(p->rack_modulator[s], 8, v.global.audio_in_nonOS[s]);
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.process(buffer.processed[0], buffer.processed[1]);
            if (imported)
                p->shared_values = v.values;
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? p->voices[c]->processed[s][p->position] * 5 : 0;
}
} // namespace surge_mm
