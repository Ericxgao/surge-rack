// SPDX-License-Identifier: GPL-3.0-or-later
#include "treemonster_dsp.h"
#include "TreeMonsterKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    treemonster::TreeMonsterStorage global;
    treemonster::TreeMonsterSettings settings;
    std::array<treemonster::TreeMonsterValue, 8> values{};
    treemonster::TreeMonsterKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[4][8]{};
    bool pending{true};
    Voice(float sr = 48000)
    {
        global.set_samplerate(sr);
        kernel.initialize_previous_sample();
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
struct TreeMonster
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<treemonster::TreeMonsterValue, 8> shared_values{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    TreeMonster()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
TreeMonster *create_treemonster()
{
    return new TreeMonster;
}
void destroy_treemonster(TreeMonster *p)
{
    delete p;
}
void treemonster_rack_mode(TreeMonster *p, bool enabled)
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
    }
}
void reset_treemonster(TreeMonster *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void treemonster_samplerate(TreeMonster *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_treemonster(p);
}
void process_treemonster(TreeMonster *p, const std::array<float, 11> &params,
                         const std::array<PolyValues, 8> &cv, const StereoPoly &input,
                         unsigned channels, std::array<PolyValues, 4> &out)
{
    const bool imported = p->rack_mode, poly = params[10] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_treemonster(p);
        p->channels = channels;
    }
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
        // FX.h reinitializes using the shared globaldata left by the last
        // processed voice, before this block's new controls and modulation.
        for (unsigned c = 0; c < channels; ++c)
            if (!p->rack_created[c])
            {
                auto &v = *p->voices[c];
                v.values = p->shared_values;
                v.kernel.init();
                p->rack_created[c] = true;
            }
        p->rack_mono->values = p->shared_values;
        p->rack_mono->kernel.init();
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &v = *p->voices[c];
            if (p->rack_created[c])
            {
                v.values = p->shared_values;
                v.kernel.init();
            }
            for (auto &b : v.input)
                std::fill_n(b, 8, 0);
            // Rack leaves pitch/envelope output buffers intact until the next
            // completed block even though the audio buffers are cleared.
            for (unsigned s = 0; s < 2; ++s)
                std::fill_n(v.processed[s], 8, 0);
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
            for (unsigned i = 0; i < 8; ++i)
            {
                float scale = i == 4 ? .1f : (i == 2 || i == 3) ? 12.f / 130 : .2f;
                v.values[i].f = imported
                                    ? (treemonster_minimum[i] + params[i] * treemonster_range[i]) +
                                          cv[i][c] * scale * treemonster_range[i]
                                    : treemonster_minimum[i] +
                                          std::clamp(params[i] + cv[i][c] * scale, 0.f, 1.f) *
                                              treemonster_range[i];
            }
            v.settings.p[2].deactivated = params[8] < .5f;
            v.settings.p[3].deactivated = params[9] < .5f;
            if (!imported && v.pending)
            {
                v.kernel.reset_native();
                v.pending = false;
            }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(buffer.input[s], 8, buffer.processed[s]);
            v.kernel.process(buffer.processed[0], buffer.processed[1]);
            if (imported)
                p->shared_values = v.values;
            for (unsigned j = 0; j < 8; ++j)
            {
                buffer.processed[2][j] =
                    .5f * (v.kernel.smoothedPitch[0][j] + v.kernel.smoothedPitch[1][j]) - 5;
                buffer.processed[3][j] =
                    .5f * (v.kernel.envelopeOut[0][j] + v.kernel.envelopeOut[1][j]) * 10;
            }
        }
    }
    for (unsigned s = 0; s < 4; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] =
                c < channels ? p->voices[c]->processed[s][p->position] * (s < 2 ? 5 : 1) : 0;
}
} // namespace surge_mm
