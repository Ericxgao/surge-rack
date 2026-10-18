// SPDX-License-Identifier: GPL-3.0-or-later
#include "ringmod_dsp.h"
#include "RingModKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    ringmod::RingModStorage global;
    ringmod::RingModSettings settings;
    std::array<ringmod::RingModValue, 9> values{};
    ringmod::RingModKernel kernel{&global, &settings, values.data()};
    sst::filters::HalfRate::HalfRateFilter carrier_filter{6, true};
    alignas(16) float input[2][8]{}, carrier[2][8]{}, processed[2][8]{};
    bool pending{true};
    Voice(float sr = 48000)
    {
        global.set_samplerate(sr);
    }
    void reset()
    {
        pending = true;
        carrier_filter.reset();
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : carrier)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct RingMod
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    bool connected{};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<ringmod::RingModValue, 9> shared_values{};
    sst::filters::HalfRate::HalfRateFilter rack_carrier_filter{6, true};
    alignas(16) float rack_carrier[2][8]{}, rack_oversampled[2][16]{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    RingMod()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
RingMod *create_ringmod()
{
    return new RingMod;
}
void destroy_ringmod(RingMod *p)
{
    delete p;
}
void ringmod_rack_mode(RingMod *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(sr);
        p->rack_mono->kernel.init();
        p->rack_mono->kernel.init_default_values();
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(sr);
        p->rack_created = {};
        p->shared_values = {};
        p->last_rack_channels = 0;
        p->position = 0;
        p->connected = false;
        p->rack_carrier_filter.reset();
        for (auto &b : p->rack_carrier)
            std::fill_n(b, 8, 0);
        for (auto &b : p->rack_oversampled)
            std::fill_n(b, 16, 0);
    }
}
void reset_ringmod(RingMod *p)
{
    p->position = 0;
    p->connected = false;
    for (auto &v : p->voices)
        v->reset();
}
void ringmod_samplerate(RingMod *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_ringmod(p);
}
bool process_ringmod(RingMod *p, const std::array<float, 14> &params,
                     const std::array<PolyValues, 9> &cv, const StereoPoly &input,
                     const StereoPoly &carrier, unsigned channels, bool connected, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[11] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_ringmod(p);
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
        p->rack_carrier_filter.reset();
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
    if (connected && !p->connected)
    {
        if (imported)
            p->rack_carrier_filter.reset();
        else
            for (auto &v : p->voices)
                v->carrier_filter.reset();
    }
    p->connected = connected;
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
        {
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
            p->voices[c]->carrier[s][p->position] = carrier[s][c] * .2f;
        }
    if (imported)
        for (unsigned s = 0; s < 2; ++s)
            p->rack_carrier[s][p->position] = carrier[s][0] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        // Keep the Mono sideband path active in Poly too. Pinned FX.h omits
        // this step in Poly, leaving the engine's audio_in buffer stale.
        if (imported)
            p->rack_carrier_filter.process_block_U2(p->rack_carrier[0], p->rack_carrier[1],
                                                    p->rack_oversampled[0], p->rack_oversampled[1],
                                                    16);
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 9; ++i)
            {
                float scale = i == 1 ? 12.f / 127 : (i == 6 || i == 7) ? 12.f / 130 : .2f;
                if (imported)
                {
                    float base = ringmod_minimum[i] + params[i] * ringmod_range[i];
                    if (i == 0 || i == 3)
                        v.values[i].i = int(std::round(base));
                    else
                        v.values[i].f = base + cv[i][c] * scale * ringmod_range[i];
                }
                else
                {
                    float value =
                        ringmod_minimum[i] +
                        std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) * ringmod_range[i];
                    if (i == 0 || i == 3)
                        v.values[i].i = int(std::round(value));
                    else
                        v.values[i].f = value;
                }
            }
            if (connected)
                v.values[0].i = 28;
            else if (imported && v.values[0].i == 28)
                v.values[0].i = 0;
            v.settings.p[6].deactivated = params[9] < .5f;
            v.settings.p[7].deactivated = params[10] < .5f;
            v.settings.p[2].extend_range = params[12] > .5f;
            v.settings.p[2].absolute = params[13] > .5f;
            if (!imported && v.pending)
            {
                v.kernel.reset_native();
                v.pending = false;
            }
            if (imported)
                for (unsigned s = 0; s < 2; ++s)
                    std::copy_n(p->rack_oversampled[s], 16, v.global.audio_in[s]);
            else
                v.carrier_filter.process_block_U2(v.carrier[0], v.carrier[1], v.global.audio_in[0],
                                                  v.global.audio_in[1], 16);
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
    return p->position == 0;
}
} // namespace surge_mm
