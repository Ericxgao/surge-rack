// SPDX-License-Identifier: GPL-3.0-or-later
#include "combulator_dsp.h"
#include "CombulatorKernel.h"
#include <memory>
#include <cstdlib>
namespace surge_mm
{
namespace
{
struct Voice
{
    combulator::CombulatorStorage global;
    combulator::CombulatorSettings settings;
    std::array<combulator::CombulatorValue, 12> values{};
    combulator::CombulatorKernel kernel{&global, &settings, values.data()};
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
struct Combulator
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<combulator::CombulatorValue, 12> shared_values{};
    combulator::CombulatorSettings shared_settings{};
    combulator::CombulatorStorage rack_random;
    bool rack_mode{};
    unsigned last_rack_channels{};
    Combulator()
    {
        for (unsigned c = 0; c < 4; ++c)
        {
            voices[c] = std::make_unique<Voice>();
            voices[c]->global.seed = 1 + 7919 * c;
        }
    }
};
Combulator *create_combulator()
{
    return new Combulator;
}
void destroy_combulator(Combulator *p)
{
    delete p;
}
void combulator_rack_mode(Combulator *p, bool enabled)
{
    p->rack_mode = enabled;
    if (enabled)
    {
        float sr = p->voices[0]->global.dsamplerate_os * .5;
        p->rack_random.seed = std::rand();
        p->rack_random.reset_random();
        auto prepare = [&](Voice &v) {
            v.global.set_samplerate(sr);
            v.global.rack_random_context = &p->rack_random;
            v.global.rack_random = [](void *ctx) {
                return static_cast<combulator::CombulatorStorage *>(ctx)->rand_pm1();
            };
            v.kernel.rack_history = true;
            v.kernel.envA = v.kernel.envR =
                std::pow(.01, 1.0 / (5 * v.global.dsamplerate_os * .001));
        };
        p->rack_mono = std::make_unique<Voice>();
        prepare(*p->rack_mono);
        p->rack_mono->kernel.init();
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = p->voices[c];
            v = std::make_unique<Voice>();
            v->global.seed = 1 + 7919 * c;
            prepare(*v);
        }
        p->rack_created = {};
        p->shared_values = {};
        p->shared_settings = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
    else
        for (auto &v : p->voices)
        {
            v->global.rack_random = nullptr;
            v->global.rack_random_context = nullptr;
            v->kernel.rack_history = false;
        }
}
void reset_combulator(Combulator *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void combulator_samplerate(Combulator *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_combulator(p);
}
void process_combulator(Combulator *p, const std::array<float, 16> &params,
                        const std::array<PolyValues, 12> &cv, const StereoPoly &input,
                        unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[15] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_combulator(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        auto initialize = [&](Voice &v) {
            v.values = p->shared_values;
            v.settings = p->shared_settings;
            v.kernel.init();
        };
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                auto &v = *p->voices[c];
                // Rack creates each poly engine at its first use. Its envelope
                // coefficients retain that creation rate even after later changes.
                v.kernel.envA = v.kernel.envR =
                    std::pow(.01, 1.0 / (5 * v.global.dsamplerate_os * .001));
                initialize(v);
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
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned id = 0; id < 12; ++id)
            {
                if (id >= 1 && id <= 3)
                {
                    bool absolute = id == 1 || params[11 + id] > .5f;
                    float lo = absolute ? combulator_low : -60,
                          hi = absolute ? combulator_high : 60;
                    // Keep the native clamp expression intact: sharing a temporary
                    // with the import path changes fast-math reassociation and can
                    // amplify pitch rounding in long feedback-comb renders.
                    if (imported)
                        v.values[id].f = lo + params[id] * (hi - lo) + 12 * cv[id][c];
                    else
                        v.values[id].f =
                            std::clamp(lo + params[id] * (hi - lo) + 12 * cv[id][c], lo, hi);
                    v.settings.p[id].extend_range = id > 1 && absolute;
                }
                else
                {
                    bool bipolar = id == 4 || id == 5 || id == 9 || id == 10;
                    float x = std::clamp(params[id] + cv[id][c] * .2f, 0.f, 1.f);
                    v.values[id].f = imported ? (bipolar ? 2 * params[id] - 1 : params[id]) +
                                                    cv[id][c] * .2f * (bipolar ? 2 : 1)
                                              : (bipolar ? 2 * x - 1 : x);
                }
            }
            v.settings.p[5].deactivated = params[12] < .5f;
            if (!imported && v.pending)
            {
                v.kernel.reset_native();
                v.pending = false;
            }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.process(buffer.processed[0], buffer.processed[1]);
            if (imported)
            {
                p->shared_values = v.values;
                p->shared_settings = v.settings;
            }
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? p->voices[c]->processed[s][p->position] * 5 : 0;
}
} // namespace surge_mm
