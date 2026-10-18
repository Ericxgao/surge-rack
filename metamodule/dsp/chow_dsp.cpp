// SPDX-License-Identifier: GPL-3.0-or-later
#include "chow_dsp.h"
#include "ChowKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    chow::ChowStorage global;
    chow::ChowSettings settings;
    std::array<chow::ChowValue, 4> values{};
    chow::ChowKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    Voice(bool deferred_init = false, float sr = 48000)
    {
        global.set_samplerate(sr);
        for (unsigned i = 0; i < 4; ++i)
        {
            settings.p[i].val_min.f = chow_minimum[i];
            settings.p[i].val_max.f = chow_minimum[i] + chow_range[i];
        }
        if (!deferred_init)
            kernel.init();
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
struct Chow
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    bool rack_mode{};
    unsigned position{}, channels{1}, last_rack_channels{};
    Chow()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Chow *create_chow()
{
    return new Chow;
}
void destroy_chow(Chow *p)
{
    delete p;
}
void chow_rack_mode(Chow *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(true, sr);
        p->rack_mono->kernel.init();
        // Allocate at state loading, but defer init until Rack would create
        // each poly engine. init() retains parts of the smoother history.
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(true, sr);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_chow(Chow *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void chow_samplerate(Chow *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_chow(p);
}
void process_chow(Chow *p, const std::array<float, 5> &params, const std::array<PolyValues, 4> &cv,
                  const StereoPoly &input, unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[4] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_chow(p);
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
            for (unsigned i : {0u, 1u, 3u})
                v.values[i].f =
                    imported ? (chow_minimum[i] + params[i] * chow_range[i]) +
                                   cv[i][c] * .2f * chow_range[i]
                             : chow_minimum[i] +
                                   std::clamp(params[i] + cv[i][c] * .2f, 0.f, 1.f) * chow_range[i];
            v.settings.p[2].val.b = params[2] > .5f || cv[2][c] > 3.f;
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
