// SPDX-License-Identifier: GPL-3.0-or-later
#include "nimbus_dsp.h"
#include "NimbusKernel.h"
#include "rack_trigger.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    nimbus::NimbusStorage global;
    nimbus::NimbusSettings settings;
    std::array<nimbus::NimbusValue, 12> values{};
    nimbus::NimbusKernel<> kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool high{}, pending{};
    RackTrigger rack_gate;
    Voice()
    {
        kernel.init();
    }
    void reset()
    {
        kernel.reset_native();
        high = false;
        pending = false;
        rack_gate = {};
        for (auto &v : input)
            std::fill_n(v, 8, 0);
        for (auto &v : processed)
            std::fill_n(v, 8, 0);
    }
};
} // namespace
struct Nimbus
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    unsigned position{}, channels{1}, last_rack_channels{};
    Nimbus()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Nimbus *create_nimbus()
{
    return new Nimbus;
}
void destroy_nimbus(Nimbus *p)
{
    delete p;
}
void prepare_nimbus_rack(Nimbus *p)
{
    if (!p->rack_mono)
    {
        p->rack_mono = std::make_unique<Voice>();
        p->rack_mono->global.set_samplerate(p->voices[0]->global.samplerate);
    }
}
void reset_nimbus(Nimbus *p)
{
    p->position = 0;
    p->last_rack_channels = 0;
    for (auto &v : p->voices)
        v->reset();
    if (p->rack_mono)
        p->rack_mono->reset();
}
void nimbus_samplerate(Nimbus *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    reset_nimbus(p);
}
void process_nimbus(Nimbus *p, const std::array<float, 13> &params,
                    const std::array<PolyValues, 10> &cv, const StereoPoly &audio,
                    unsigned channels, const PolyValues &freeze, const PolyValues &trigger,
                    StereoPoly &out, bool rack_trigger)
{
    const bool rack_poly = params[12] > .5f;
    if (rack_trigger)
    {
        // FX::processPoly reinitializes on width changes. NimbusEffect::init
        // resets the output FIFO, but retains Clouds audio, SRC state and stubs.
        // Sum/Poly share wrapper buffers and trigger history, not DSP engines.
        if (rack_poly && channels != p->last_rack_channels)
        {
            p->last_rack_channels = channels;
            p->position = 0;
            p->rack_mono->kernel.init();
            for (auto &v : p->voices)
            {
                v->kernel.init();
                for (auto &b : v->input)
                    std::fill_n(b, 8, 0);
                for (auto &b : v->processed)
                    std::fill_n(b, 8, 0);
            }
        }
    }
    else if (channels != p->channels)
    {
        p->position = 0;
        for (auto &v : p->voices)
            v->reset();
        p->channels = channels;
    }
    for (unsigned c = 0; c < channels; ++c)
    {
        auto &v = *p->voices[c];
        if (!rack_trigger)
        {
            if (v.high)
            {
                if (trigger[c] <= .1f)
                    v.high = false;
            }
            else if (trigger[c] >= 1)
            {
                v.high = true;
                v.pending = true;
            }
        }
        for (unsigned s = 0; s < 2; ++s)
            v.input[s][p->position] = audio[s][c] * .2f;
    }
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = rack_trigger && !rack_poly ? *p->rack_mono : buffer;
            v.values[0].i = int(std::round(std::clamp(params[0], 0.f, 1.f) * 3));
            v.values[1].i = int(std::round(std::clamp(params[1], 0.f, 1.f) * 3));
            for (unsigned i = 0; i < 9; ++i)
                v.values[nimbus_cv_params[i]].f =
                    nimbus_minimum[i] +
                    std::clamp(params[nimbus_cv_params[i]] + cv[i][c] * .2f, 0.f, 1.f) *
                        nimbus_range[i];
            v.values[8].f = ((params[8] > .5f || freeze[c] > 3) ? 1.f : 0.f) + cv[9][c] * .2f;
            v.kernel.nativeTriggerLatch = !rack_trigger;
            v.kernel.setNimbusTrigger(rack_trigger ? buffer.rack_gate.process(trigger[c])
                                                   : v.pending);
            v.pending = false;
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
