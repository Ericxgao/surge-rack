// SPDX-License-Identifier: GPL-3.0-or-later
#include "distortion_dsp.h"
#include "DistortionKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
void warm_shapers()
{
    static const bool ready = []() {
        for (int i = 0; i < distortion::n_fxws; ++i)
        {
            sst::waveshapers::QuadWaveshaperState state{};
            distortion::initialize_shaper(state, distortion::FXWaveShapers[i]);
            auto fn = sst::waveshapers::GetQuadWaveshaper(distortion::FXWaveShapers[i]);
            fn(&state, _mm_setzero_ps(), _mm_set1_ps(1));
        }
        return true;
    }();
    (void)ready;
}
struct Voice
{
    distortion::DistortionStorage global;
    distortion::DistortionSettings settings;
    std::array<distortion::DistortionValue, 12> values{};
    distortion::DistortionKernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    bool pending{true};
    int model{-1};
    Voice(float sr = 48000)
    {
        global.set_samplerate(sr);
    }
    void reset()
    {
        pending = true;
        model = -1;
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Distortion
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    std::array<distortion::DistortionValue, 12> shared_values{};
    distortion::DistortionSettings shared_settings{};
    bool rack_mode{};
    unsigned last_rack_channels{};
    Distortion()
    {
        warm_shapers();
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Distortion *create_distortion()
{
    return new Distortion;
}
void destroy_distortion(Distortion *p)
{
    delete p;
}
void distortion_rack_mode(Distortion *p, bool enabled)
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
        p->shared_settings = p->rack_mono->settings;
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_distortion(Distortion *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void distortion_samplerate(Distortion *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    if (!p->rack_mode)
        reset_distortion(p);
}
void process_distortion(Distortion *p, const std::array<float, 18> &params,
                        const std::array<PolyValues, 12> &cv, const StereoPoly &input,
                        unsigned channels, StereoPoly &out)
{
    const bool imported = p->rack_mode, poly = params[14] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_distortion(p);
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
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 12; ++i)
            {
                if (imported)
                {
                    float base = distortion_minimum[i] + params[i] * distortion_range[i];
                    if (i == 11)
                    {
                        v.values[i].i = int(std::round(base));
                        v.settings.p[i].val.i = v.values[i].i;
                    }
                    else
                    {
                        v.values[i].f =
                            base + cv[i][c] * distortion_cv_scale(i) * distortion_range[i];
                        v.settings.p[i].val.f = base;
                    }
                }
                else
                {
                    v.values[i].f =
                        distortion_minimum[i] +
                        std::clamp(params[i] + cv[i][c] * distortion_cv_scale(i), 0.f, 1.f) *
                            distortion_range[i];
                    v.settings.p[i].val = v.values[i];
                }
            }
            if (!imported)
                v.values[11].i = int(std::round(v.values[11].f));
            v.settings.p[3].deactivated = params[12] < .5f;
            v.settings.p[9].deactivated = params[13] < .5f;
            v.settings.p[0].multiplier = params[15] > .5f ? 3 : 1;
            v.settings.p[6].multiplier = params[16] > .5f ? 3 : 1;
            v.settings.p[4].multiplier = params[17] > .5f ? 5 : 1;
            if (!imported && v.pending)
            {
                v.kernel.hr_a.reset();
                v.kernel.hr_b.reset();
                v.kernel.drive.set_target(0);
                v.kernel.drive.instantize();
                v.kernel.outgain.set_target(0);
                v.kernel.outgain.instantize();
                v.kernel.init();
                v.pending = false;
            }
            if (!imported && v.model != v.values[11].i)
            {
                v.model = v.values[11].i;
                distortion::initialize_shaper(v.kernel.wsState, distortion::FXWaveShapers[v.model]);
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
