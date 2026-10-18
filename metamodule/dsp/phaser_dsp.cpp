// SPDX-License-Identifier: GPL-3.0-or-later
#include "phaser_dsp.h"
#include "fx_clock.h"
#include "globals.h"
#include "effect_config.h"
#include <sst/effects/Phaser.h>
#include <memory>
namespace surge_mm
{
using Kernel = sst::effects::phaser::Phaser<EffectConfig>;
namespace
{
const auto &fresh_modulation()
{
    // FXModControl has no state-only reset. Cache its original initialized
    // state before audio starts, including the immutable sine table, so reset
    // can copy it without allocations or recomputing 8192 sine values.
    static const decltype(Kernel::modLFOL) initial;
    return initial;
}
struct Voice
{
    EffectGlobal global;
    EffectSettings settings;
    std::array<float, 12> values{};
    Kernel kernel{&global, &settings, values.data()};
    alignas(16) float input[2][8]{}, processed[2][8]{};
    Voice()
    {
        (void)fresh_modulation();
        // The original engine grows its filter bank when Stages increases.
        // Allocate every legal stage before entering the audio callback.
        values[Kernel::ph_stages] = 16;
        kernel.init_stages();
        values[Kernel::ph_stages] = 4;
        kernel.init_stages();
        kernel.initialize();
        // Also materialize the shared pitch tables outside audio processing.
        (void)FilterTuning::note_to_pitch(0);
    }
    void reset()
    {
        kernel.modLFOL = fresh_modulation();
        kernel.modLFOR = fresh_modulation();
        kernel.feedback.reset();
        kernel.tone.reset();
        kernel.width.set_target(0);
        kernel.width.instantize();
        kernel.mix.set_target(0);
        kernel.mix.instantize();
        kernel.initialize();
        for (auto &v : input)
            std::fill(std::begin(v), std::end(v), 0);
        for (auto &v : processed)
            std::fill(std::begin(v), std::end(v), 0);
    }
};
} // namespace
struct Phaser
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    std::unique_ptr<Voice> rack_mono;
    std::array<bool, 4> rack_created{};
    FXClock tempo;
    unsigned position{}, channels{1}, last_rack_channels{};
    std::array<float, 12> minimum{}, range{};
    Phaser()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
        for (int i = 0; i < 12; ++i)
        {
            auto m = voices[0]->kernel.paramAt(i);
            minimum[i] = m.minVal;
            range[i] = m.maxVal - m.minVal;
        }
    }
};
Phaser *create_phaser()
{
    return new Phaser;
}
void destroy_phaser(Phaser *p)
{
    delete p;
}
void phaser_rack_clock(Phaser *p, bool enabled)
{
    p->tempo.set_rack_mode(enabled);
    if (enabled)
    {
        // Rack owns distinct Sum and Poly engines. Allocate/reconstruct them at
        // patch loading, retaining all stage capacity outside audio processing.
        float sr = p->voices[0]->global.samplerate;
        auto fresh = [sr](std::unique_ptr<Voice> &v) {
            v = std::make_unique<Voice>();
            v->global.set_samplerate(sr);
            v->kernel.initialize();
        };
        fresh(p->rack_mono);
        for (auto &v : p->voices)
            fresh(v);
        p->rack_created = {};
        p->last_rack_channels = 0;
        p->position = 0;
    }
}
void reset_phaser(Phaser *p)
{
    p->position = 0;
    p->tempo.reset_engine();
    for (auto &v : p->voices)
        v->reset();
}
void phaser_samplerate(Phaser *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
    {
        v->global.set_samplerate(sr);
        if (!p->tempo.rack_mode)
            v->kernel.onSampleRateChanged();
    }
    if (p->rack_mono)
        p->rack_mono->global.set_samplerate(sr);
    // Rack's FX callback updates storage/clock only; it does not invoke the
    // effect's sampleRateReset or clear the shared audio buffers.
    p->tempo.sample_rate_changed();
    if (!p->tempo.rack_mode)
        reset_phaser(p);
}
void process_phaser(Phaser *p, const std::array<float, 16> &params,
                    const std::array<PolyValues, 12> &cv, const StereoPoly &input,
                    unsigned channels, float clock, bool connected, StereoPoly &out)
{
    const bool imported = p->tempo.rack_mode, poly = params[14] > .5f;
    if (!imported && channels != p->channels)
    {
        reset_phaser(p);
        p->channels = channels;
    }
    p->tempo.process(clock, connected, params[15] > .5f, p->voices[0]->global.samplerate);
    if (imported && poly && channels != p->last_rack_channels)
    {
        p->last_rack_channels = channels;
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
                std::fill(std::begin(b), std::end(b), 0);
            for (auto &b : v.processed)
                std::fill(std::begin(b), std::end(b), 0);
        }
        p->position = 0;
    }
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    // Match Rack FX's block adapter: output sample zero on the eighth input,
    // yielding seven samples of latency (also at Mix=0).
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &buffer = *p->voices[c];
            auto &v = imported && !poly ? *p->rack_mono : buffer;
            for (unsigned i = 0; i < 12; ++i)
            {
                float normalized = std::clamp(params[i] + cv[i][c] / 5, 0.f, 1.f);
                v.values[i] = p->minimum[i] + normalized * p->range[i];
            }
            v.settings.deactivated[Kernel::ph_tone] = params[12] < .5f;
            v.settings.deactivated[Kernel::ph_mod_rate] = params[13] < .5f;
            v.settings.synced[Kernel::ph_mod_rate] = p->tempo.synced;
            v.global.tempo_ratio = p->tempo.ratio;
            // Rack bounds/snaps the base Parameter before adding its CV in
            // globaldata. Snapping the modulated result would staircase rate CV.
            if (p->tempo.synced)
            {
                constexpr auto r = Kernel::ph_mod_rate;
                v.values[r] = std::clamp(round_tempo_time(p->minimum[r] + params[r] * p->range[r]) +
                                             cv[r][c] / 5 * p->range[r],
                                         p->minimum[r], p->minimum[r] + p->range[r]);
            }
            if (imported)
                for (unsigned i = 0; i < 12; ++i)
                {
                    // Integer controls remain bounded, including their new direct
                    // CV jacks. Rack export omits modulation of integer parameters.
                    if (i == Kernel::ph_stages || i == Kernel::ph_mod_wave)
                        continue;
                    float base = p->minimum[i] + params[i] * p->range[i];
                    if (i == Kernel::ph_mod_rate && p->tempo.synced)
                        base = round_tempo_time(base);
                    v.values[i] = base + cv[i][c] / 5 * p->range[i];
                }
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
