// SPDX-License-Identifier: GPL-3.0-or-later
#include "spring_dsp.h"
#include "spring/SpringKernel.h"
#include "rack_trigger.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    spring::SpringStorage global;
    spring::SpringSettings settings;
    std::array<spring::SpringValue, 8> values{};
    spring::SpringKernel<> kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{};
    explicit Voice(unsigned seed)
    {
        kernel.proc.seed = seed;
        kernel.init();
    }
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
struct SpringReverb
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> knock{};
    std::array<RackTrigger, 4> rack_gates;
    RackTrigger rack_button;
    unsigned position{}, channels{1}, last_rack_channels{};
    SpringReverb()
    {
        for (unsigned c = 0; c < 4; ++c)
            voices[c] = std::make_unique<Voice>(1 + 7919 * c);
    }
};
SpringReverb *create_spring()
{
    return new SpringReverb;
}
void destroy_spring(SpringReverb *p)
{
    delete p;
}
void spring_rack_mode(SpringReverb *p, bool enabled)
{
    if (enabled && !p->rack_mono)
    {
        p->rack_mono = std::make_unique<Voice>(1);
        p->rack_mono->global.set_samplerate(p->voices[0]->global.samplerate);
        p->rack_mono->reset();
    }
    for (auto &v : p->voices)
        v->kernel.proc.nativeReset = !enabled;
    if (p->rack_mono)
        p->rack_mono->kernel.proc.nativeReset = !enabled;
}
void reset_spring_audio(SpringReverb *p)
{
    p->position = 0;
    p->last_rack_channels = 0;
    p->knock.fill(false);
    for (auto &v : p->voices)
        v->reset();
    if (p->rack_mono)
        p->rack_mono->reset();
}
void reset_spring(SpringReverb *p)
{
    reset_spring_audio(p);
    p->rack_gates = {};
    p->rack_button = {};
}
void spring_samplerate(SpringReverb *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    reset_spring(p);
}
void process_spring(SpringReverb *p, const std::array<float, 9> &params,
                    const std::array<PolyValues, 8> &cv, const std::array<bool, 4> &knock,
                    const StereoPoly &input, unsigned channels, StereoPoly &out,
                    const PolyValues &gate, bool rack_trigger)
{
    const bool rack_poly = params[8] > .5f;
    if (rack_trigger)
    {
        if (rack_poly && channels != p->last_rack_channels)
        {
            p->position = 0;
            p->last_rack_channels = channels;
            p->rack_mono->kernel.init();
            p->rack_mono->pending = false;
            for (auto &v : p->voices)
            {
                v->kernel.init();
                v->pending = false;
                for (auto &b : v->input)
                    std::fill_n(b, 8, 0);
                for (auto &b : v->processed)
                    std::fill_n(b, 8, 0);
            }
        }
    }
    else if (channels != p->channels)
    {
        reset_spring(p);
        p->channels = channels;
    }
    for (unsigned c = 0; c < channels; ++c)
    {
        p->knock[c] = p->knock[c] || knock[c];
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    }
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = rack_trigger && !rack_poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 7; ++i)
            {
                unsigned id = spring_cv_ids[i];
                v.values[id].f = std::clamp(params[id] + cv[i][c] * .2f, 0.f, 1.f);
            }
            bool strike = p->knock[c];
            if (rack_trigger)
            {
                bool triggered = p->rack_gates[c].process(gate[c]);
                bool pressed = p->rack_button.process(params[6], .5f);
                strike = triggered || pressed;
            }
            v.values[6].f = (strike ? 1.f : 0.f) + cv[7][c] * .2f;
            p->knock[c] = false;
            if (v.pending)
            {
                v.kernel.init();
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
