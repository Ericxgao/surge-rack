// SPDX-License-Identifier: GPL-3.0-or-later
#include "bonsai_dsp.h"
#include "globals.h"
#include "bonsai_config.h"
#include <sst/effects/Bonsai.h>
#include <memory>
namespace surge_mm
{
namespace
{
using Kernel = sst::effects::bonsai::Bonsai<BonsaiConfig>;
struct Voice
{
    EffectGlobal global;
    BonsaiSettings settings;
    std::array<float, 11> values{};
    Kernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    Voice(float sr = 48000)
    {
        global.set_samplerate(sr);
        kernel.initialize();
    }
    void reset()
    {
        kernel.initialize();
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Bonsai
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    std::array<float, 11> minimum{}, range{};
    Bonsai()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
        for (int i = 0; i < 11; ++i)
        {
            auto m = voices[0]->kernel.paramAt(i);
            minimum[i] = m.minVal;
            range[i] = m.maxVal - m.minVal;
        }
    }
};
Bonsai *create_bonsai()
{
    return new Bonsai;
}
void destroy_bonsai(Bonsai *p)
{
    delete p;
}
void bonsai_rack_mode(Bonsai *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(sr);
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(sr);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_bonsai(Bonsai *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void bonsai_samplerate(Bonsai *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_bonsai(p);
}
void process_bonsai(Bonsai *p, const std::array<float, 14> &params,
                    const std::array<PolyValues, 11> &cv, const StereoPoly &input,
                    unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[11] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_bonsai(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        // Bonsai initialize() clears all filter/smoother/noise history and
        // samples the current rate. Rack runs it only on poly width changes,
        // not merely on entering Sum or receiving a sample-rate callback.
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                p->voices[c]->kernel.initialize();
                p->rack_created[c] = true;
            }
        p->rack_mono->kernel.initialize();
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
                v.kernel.initialize();
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
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 11; ++i)
            {
                if (imported)
                {
                    float base = p->minimum[i] + params[i] * p->range[i];
                    v.values[i] =
                        i == 3 || i == 4 ? std::round(base) : base + cv[i][c] * .2f * p->range[i];
                }
                else
                    v.values[i] = p->minimum[i] +
                                  std::clamp(params[i] + cv[i][c] * .2f, 0.f, 1.f) * p->range[i];
            }
            v.settings.bass_mode = params[12] > .5f;
            v.settings.extended[1] = params[13] > .5f;
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
