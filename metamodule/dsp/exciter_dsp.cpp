// SPDX-License-Identifier: GPL-3.0-or-later
#include "exciter_dsp.h"
#include "ExciterKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    exciter::ExciterStorage global;
    exciter::ExciterSettings settings;
    std::array<exciter::ExciterValue, 5> values{};
    exciter::ExciterKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    Voice(bool imported = false, float sr = 48000)
    {
        global.set_samplerate(sr);
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
// Preserve src/FX.h's post-unscale cubic soft clip, including double arithmetic.
float rack_output(float x)
{
    x = std::clamp(x * (1.f / exciter_input_scale), -1.5f, 1.5f);
    x = x - 4.0 / 27.0 * x * x * x;
    return x * 5;
}
} // namespace
struct Exciter
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    bool rack_mode{};
    unsigned position{}, channels{1}, last_rack_channels{};
    Exciter()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Exciter *create_exciter()
{
    return new Exciter;
}
void destroy_exciter(Exciter *p)
{
    delete p;
}
void exciter_rack_mode(Exciter *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.samplerate;
        p->rack_mono = std::make_unique<Voice>(true, sr);
        for (auto &v : p->voices)
            v = std::make_unique<Voice>(true, sr);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_exciter(Exciter *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void exciter_samplerate(Exciter *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_exciter(p);
}
void process_exciter(Exciter *p, const std::array<float, 6> &params,
                     const std::array<PolyValues, 5> &cv, const StereoPoly &input,
                     unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[5] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_exciter(p);
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
            p->voices[c]->input[s][p->position] = input[s][c] * .2f * exciter_input_scale;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 5; ++i)
            {
                float value = params[i] + cv[i][c] * .2f;
                v.values[i].f =
                    (i == 2 || i == 3)
                        ? exciter_time_min + std::clamp(value, -.5f, 1.5f) * exciter_time_range
                        : std::clamp(value, 0.f, 1.f);
            }
            if (imported)
                for (unsigned i = 0; i < 5; ++i)
                {
                    float base = (i == 2 || i == 3)
                                     ? exciter_time_min + params[i] * exciter_time_range
                                     : params[i];
                    v.values[i].f =
                        base + cv[i][c] * .2f * ((i == 2 || i == 3) ? exciter_time_range : 1.f);
                }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.process(buffer.processed[0], buffer.processed[1]);
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? rack_output(p->voices[c]->processed[s][p->position]) : 0;
}
} // namespace surge_mm
