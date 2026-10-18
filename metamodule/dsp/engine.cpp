// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "AliasOscillator.h"
#include "StringOscillator.h"
#include "TwistOscillator.h"
#include "WavetableOscillator.h"
#include "WindowOscillator.h"
#include "table_data_internal.h"
#include "FM2Oscillator.h"
#include "FM3Oscillator.h"
#include "ModernOscillator.h"
#include "SineOscillator.h"
#include "ClassicOscillator.h"
#include "SampleAndHoldOscillator.h"
#include "sst/filters/HalfRateFilter.h"
#include <memory>
// Base methods from Surge Oscillator.cpp; its full factory pulls in the synth.
Oscillator::Oscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p)
    : storage(s), oscdata(o), localcopy(p), master_osc(nullptr), drift(0), ticker(0)
{
}
Oscillator::~Oscillator() = default;
namespace surge_mm
{
struct OscillatorExtras
{
    std::array<float, alias_option_count> options{};
    alignas(16) float input[8]{}, audio[2][64]{};
    sst::filters::HalfRate::HalfRateFilter up{6, true};
};
struct Engine
{
    Model model;
    TableHandle table;
    std::array<float, wavetable_option_count> table_options{1, 0, 0, 1, 0, 0, .5f, .5f};
    std::unique_ptr<OscillatorExtras> extra;
    SurgeStorage storage;
    OscillatorStorage oscdata;
    std::array<pdata, 7> local{};
    std::unique_ptr<Oscillator> oscillator;
    std::array<float, parameter_count> params{};
    std::array<float, input_count> inputs{};
    sst::filters::HalfRate::HalfRateFilter halfband{6, true};
    OscillatorOutputSettings output_settings;
    std::array<float, 2> dc_x{}, dc_y{};
    alignas(16) float left[block_size]{};
    alignas(16) float right[block_size]{};
    int position{block_size}, unison{1};
    bool reset_pending{true}, reset_high{}, trigger_connected{}, rack_input_routing{};
    float octave_shift{};
    std::array<float, 9> standard_options{.5f, 0, 0, .5f, 1, 0, 0, 0, 0};
    std::array<float, twist_option_count> twist_options{0, 0, .5f, 1};
    explicit Engine(Model m) : model(m)
    {
        for (int i = 0; i < 7; ++i)
            oscdata.p[i].param_id_in_scene = i;
        oscdata.retrigger.val.b = true;
        if (m == Model::Wavetable || m == Model::Window)
        {
            table = default_wavetable();
            oscdata.wt.bind(table->table);
            if (m == Model::Window)
                storage.WindowWT.bind(window_wavetable()->table);
        }
        switch (m)
        {
        case Model::Wavetable:
            oscillator = std::make_unique<WavetableOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Window:
            oscillator = std::make_unique<WindowOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Twist:
            oscillator = std::make_unique<TwistOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::String:
            oscillator = std::make_unique<StringOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Alias:
            oscillator = std::make_unique<AliasOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::FM2:
            oscillator = std::make_unique<FM2Oscillator>(&storage, &oscdata, local.data());
            break;
        case Model::FM3:
            oscillator = std::make_unique<FM3Oscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Modern:
            oscillator = std::make_unique<ModernOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Sine:
            oscillator = std::make_unique<SineOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::Classic:
            oscillator = std::make_unique<ClassicOscillator>(&storage, &oscdata, local.data());
            break;
        case Model::SHNoise:
            oscillator =
                std::make_unique<SampleAndHoldOscillator>(&storage, &oscdata, local.data());
            break;
        }
        oscillator->init_ctrltypes();
        oscillator->init_default_values();
        if (m == Model::Alias || m == Model::String)
        {
            extra = std::make_unique<OscillatorExtras>();
            storage.audio_in = extra->audio;
        }
        if (m == Model::String)
        {
            auto *string = static_cast<StringOscillator *>(oscillator.get());
            string->ownDelayLines = true;
            for (auto &line : string->delayLine)
                line = new SSESincDelayLine<16384>(storage.sinctable);
            oscdata.p[6].deform_type = StringOscillator::filter_compensate;
            extra->options = {0, 1, 1, 0, 0, 0, 0, .5f, 1};
        }
        if (m == Model::Alias)
        {
            oscillator->init_extra_config();
            for (int i = 0; i < 16; ++i)
                extra->options[i] = .5f + .5f / (i + 1);
            extra->options[19] = .5f;
            extra->options[20] = 1;
            // Initialize shared TX tables before entering the audio thread.
            oscillator->init(60);
        }
        if (m == Model::Window)
            oscdata.p[0].extend_range = true;
        if (m == Model::Twist)
        {
            static_cast<TwistOscillator *>(oscillator.get())->useCorrectLPGBlockSize = true;
            oscdata.p[5].val.f = oscdata.p[6].val.f = .5f;
        }
        params[0] = .5f;
        for (int i = 0; i < 7; ++i)
            params[i + 1] = oscdata.p[i].normalized();
        params[9] = 1.f;
        // Normalized 0.5 is zero bipolar modulation depth.
        std::fill(params.begin() + base_parameter_count, params.end(), .5f);
    }
};
Engine *create(Model model)
{
    return new Engine(model);
}
void destroy(Engine *e)
{
    delete e;
}
void set_octave_shift(Engine *e, float value)
{
    if (std::isfinite(value))
        e->octave_shift = 6 * (std::clamp(value, 0.f, 1.f) - .5f);
}
float get_octave_shift(const Engine *e)
{
    return e->octave_shift / 6 + .5f;
}
void set_standard_option(Engine *e, int id, float v)
{
    if (id < 0 || id >= int(standard_option_count(e->model)) || !std::isfinite(v))
        return;
    v = std::clamp(v, 0.f, 1.f);
    if (e->standard_options[id] == v)
        return;
    e->standard_options[id] = v;
    if (id == 0)
        set_octave_shift(e, v);
    if (id == 1)
        e->oscdata.p[fm_oscillator(e->model) ? 6 : 5].extend_range = v > .5f;
    if (id == 2)
    {
        if (fm_oscillator(e->model))
            e->oscdata.p[6].deform_type = v > .5f ? 1 : 0;
        else
            e->oscdata.p[5].absolute = v > .5f;
    }
    if (id == 3)
    {
        e->storage.patch.character.val.i = int(std::round(v * 2));
        e->reset_pending = true;
    }
    if (id == 4)
        e->oscdata.retrigger.val.b = v > .5f;
    if (id >= 5 && e->model == Model::Modern)
        e->oscdata.p[ModernOscillator::mo_tri_mix].deform_type =
            int(std::round(e->standard_options[5] * 2)) |
            (e->standard_options[6] > .5f ? ModernOscillator::mo_subone : 0) |
            (e->standard_options[7] > .5f ? ModernOscillator::mo_subskipsync : 0);
    if (e->model == Model::Sine)
    {
        if (id == 7)
            e->oscdata.p[SineOscillator::sine_feedback].deform_type = v > .5f ? 1 : 0;
        if (id == 8)
            e->oscdata.p[SineOscillator::sine_feedback].extend_range = v > .5f;
    }
    if (e->model == Model::FM3 && (id == 5 || id == 6))
        e->oscdata.p[id == 5 ? 1 : 3].absolute = v > .5f;
}
float get_standard_option(const Engine *e, int id)
{
    return id >= 0 && id < int(e->standard_options.size()) ? e->standard_options[id] : 0;
}
bool configure_output(Engine *e, OscillatorOutputSettings settings)
{
    if (settings.halfband_order < 1 || settings.halfband_order > 6)
        return false;
    if (settings.halfband_order != e->output_settings.halfband_order ||
        settings.steep != e->output_settings.steep)
    {
        using Halfband = sst::filters::HalfRate::HalfRateFilter;
        e->halfband.~Halfband();
        new (&e->halfband) Halfband(settings.halfband_order, settings.steep);
    }
    if (settings.dc_block != e->output_settings.dc_block)
    {
        e->dc_x.fill(0);
        e->dc_y.fill(0);
    }
    e->output_settings = settings;
    return true;
}
void sample_rate(Engine *e, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    e->storage.setSamplerate(sr);
    e->reset_pending = true;
    e->position = block_size;
    e->halfband.reset();
    e->dc_x.fill(0);
    e->dc_y.fill(0);
    if (e->extra)
    {
        e->extra->up.reset();
        std::fill(std::begin(e->extra->input), std::end(e->extra->input), 0);
        for (auto &channel : e->extra->audio)
            std::fill(std::begin(channel), std::end(channel), 0);
    }
}
void set_param(Engine *e, int id, float v)
{
    if (id < 0 || id >= parameter_count || !std::isfinite(v))
        return;
    e->params[id] = std::clamp(v, 0.f, 1.f);
}
float get_param(const Engine *e, int id)
{
    return id >= 0 && id < parameter_count ? e->params[id] : 0.f;
}
const char *param_name(const Engine *e, int id)
{
    if (id == 0)
        return "Pitch";
    if (id > 0 && id < 8)
        return id == 3 && e->oscdata.p[2].ctrltype == ct_modern_trimix ? "Triangle"
                                                                       : e->oscdata.p[id - 1].name;
    if (id == 8)
        return "Drift";
    if (id == 9)
        return "Level";
    return "";
}
void set_wavetable(Engine *e, const TableHandle &table)
{
    if (!table || (e->model != Model::Wavetable && e->model != Model::Window))
        return;
    e->table = table;
    e->oscdata.wt.bind(table->table);
    e->reset_pending = true;
}
void set_wavetable_option(Engine *e, int id, float v)
{
    if ((e->model != Model::Wavetable && e->model != Model::Window) || id < 0 ||
        id >= wavetable_option_count || !std::isfinite(v))
        return;
    v = std::clamp(v, 0.f, 1.f);
    if (e->table_options[id] == v)
        return;
    e->table_options[id] = v;
    if (id == 0)
        e->oscdata.p[0].extend_range = v > .5f;
    else if (id == 1)
        e->oscdata.p[5].extend_range = v > .5f;
    else if (id == 2)
        e->oscdata.p[5].absolute = v > .5f;
    else if (id == 3)
        e->oscdata.retrigger.val.b = v > .5f;
    else if (id == 6)
        set_octave_shift(e, v);
    else if (id == 7)
    {
        e->storage.patch.character.val.i = int(std::round(v * 2));
        e->reset_pending = true;
    }
    else if (e->model == Model::Window)
        e->oscdata.p[id - 1].deactivated = v <= .5f;
}
float get_wavetable_option(const Engine *e, int id)
{
    return id >= 0 && id < wavetable_option_count ? e->table_options[id] : 0;
}
void set_trigger_connected(Engine *e, bool connected)
{
    e->trigger_connected = connected;
}
void set_rack_input_routing(Engine *e, bool enabled)
{
    e->rack_input_routing = enabled;
}
void set_twist_option(Engine *e, int id, float v)
{
    if (e->model != Model::Twist || id < 0 || id >= twist_option_count || !std::isfinite(v))
        return;
    v = std::clamp(v, 0.f, 1.f);
    if (e->twist_options[id] == v)
        return;
    e->twist_options[id] = v;
    if (id == 1)
        e->oscdata.p[4].extend_range = v > .5f;
    else if (id == 2)
    {
        e->storage.patch.character.val.i = int(std::round(v * 2));
        e->reset_pending = true;
    }
    else if (id == 3)
        e->oscdata.retrigger.val.b = v > .5f;
}
float get_twist_option(const Engine *e, int id)
{
    return e->model == Model::Twist && id >= 0 && id < twist_option_count ? e->twist_options[id]
                                                                          : 0;
}
bool twist_lpg(const Engine *e)
{
    const float value = e->twist_options[0];
    const int mode = value < .25f ? 0 : value <= .75f ? 1 : 2;
    return mode == 2 || (mode == 0 && e->trigger_connected);
}
void set_string_option(Engine *e, int id, float v)
{
    if (e->model != Model::String || id < 0 || id >= string_option_count || !std::isfinite(v))
        return;
    v = std::clamp(v, 0.f, 1.f);
    if (e->extra->options[id] == v)
        return;
    e->extra->options[id] = v;
    using S = StringOscillator;
    auto &p = e->oscdata.p;
    if (id == 0)
    {
        p[1].deform_type = (p[1].deform_type & ~S::os_all) | (v > .5f ? S::os_twox : S::os_onex);
        e->reset_pending = true;
    }
    else if (id == 1)
    {
        constexpr int modes[]{S::interp_zoh, S::interp_lin, S::interp_sinc};
        p[1].deform_type = (p[1].deform_type & ~S::interp_all) | modes[int(std::round(v * 2))];
    }
    else if (id == 2)
    {
        constexpr int modes[]{S::filter_fixed, S::filter_keytrack, S::filter_compensate};
        p[6].deform_type = modes[int(std::round(v * 2))];
    }
    else if (id == 3 || id == 4)
        p[id - 1].extend_range = v > .5f;
    else if (id == 5)
        p[4].extend_range = v > .5f;
    else if (id == 6)
        p[4].absolute = v > .5f;
    else if (id == 7)
    {
        e->storage.patch.character.val.i = int(std::round(v * 2));
        e->reset_pending = true;
    }
    else if (id == 8)
        e->oscdata.retrigger.val.b = v > .5f;
}
float get_string_option(const Engine *e, int id)
{
    return e->model == Model::String && id >= 0 && id < string_option_count ? e->extra->options[id]
                                                                            : 0;
}
void set_alias_option(Engine *e, int id, float v)
{
    if (e->model != Model::Alias || id < 0 || id >= alias_option_count || !std::isfinite(v))
        return;
    v = std::clamp(v, 0.f, 1.f);
    if (e->extra->options[id] == v)
        return;
    e->extra->options[id] = v;
    if (id < 16)
        e->oscdata.extraConfig.data[id] = v * 2 - 1;
    else if (id == 16)
        e->oscdata.p[2].deform_type = v > .5f;
    else if (id == 17)
        e->oscdata.p[5].extend_range = v > .5f;
    else if (id == 18)
        e->oscdata.p[5].absolute = v > .5f;
    else if (id == 19)
    {
        e->storage.patch.character.val.i = int(std::round(v * 2));
        e->reset_pending = true;
    }
    else if (id == 20)
        e->oscdata.retrigger.val.b = v > .5f;
}
float get_alias_option(const Engine *e, int id)
{
    return e->extra && id >= 0 && id < alias_option_count ? e->extra->options[id] : 0;
}
void input(Engine *e, int id, float v)
{
    if (id < 0 || id >= input_count || !std::isfinite(v))
        return;
    e->inputs[id] = std::clamp(v, -10.f, 10.f);
    if (id == 1)
    {
        if (v >= 1.f && !e->reset_high)
        {
            if (e->model != Model::Twist || !twist_lpg(e))
                e->reset_pending = true;
            e->reset_high = true;
        }
        else if (v <= .1f)
            e->reset_high = false;
    }
}
void process(Engine *e, float &left, float &right)
{
    if (e->position == block_size)
    {
        std::array<float, modulation_targets> mod{};
        for (int target = 0; target < modulation_targets; ++target)
            for (int source = 0; source < modulation_sources; ++source)
                // Match Rack's ModulationAssistant: full range per 10V.
                // Pitch's 120-semitone range makes full depth 1 octave/volt.
                mod[target] += (2.f * e->params[modulation_param(target, source)] - 1.f) *
                               e->inputs[modulation_input_start + source] * .1f;
        for (int i = 0; i < 7; ++i)
        {
            auto &p = e->oscdata.p[i];
            p.set_normalized(e->params[i + 1] + e->inputs[i + 2] / 5.f + mod[i + 1]);
            if (p.ctrltype == ct_freq_audible_deactivatable_hp && e->model != Model::Window)
                p.deactivated = p.val.f <= p.val_min.f;
            if (p.ctrltype == ct_freq_audible_deactivatable_lp && e->model != Model::Window)
                p.deactivated = p.val.f >= p.val_max.f;
            if (e->model == Model::Sine || e->model == Model::SHNoise)
            {
                const int lowcut = e->model == Model::Sine ? 3 : 2;
                if (i == lowcut || i == lowcut + 1)
                {
                    // Imported Rack switches use a strict > 0.5 threshold.
                    // Their .5..1 mapping must keep its midpoint (.75) Off.
                    const auto setting = e->standard_options[5 + i - lowcut];
                    const unsigned mode = setting < .25f ? 0 : setting <= .75f ? 1 : 2;
                    if (mode)
                        p.deactivated = mode == 1;
                }
            }
            e->local[i] = p.val;
            if (p.ctrltype == ct_osccount && e->unison != p.val.i)
            {
                e->unison = p.val.i;
                e->reset_pending = true;
            }
        }
        if (e->model == Model::Alias)
        {
            constexpr int waves[]{0, 1, 2, 3, 17, 9, 10, 11, 12, 13, 14, 15, 16};
            e->oscdata.p[0].val.i = waves[e->oscdata.p[0].val.i];
        }
        if (e->extra)
        {
            auto &audio = e->extra->audio;
            int offset = 0;
            if (e->model == Model::Alias)
            {
                for (int c = 0; c < 2; ++c)
                    std::move(audio[c] + 16, audio[c] + 64, audio[c]);
                offset = 48;
            }
            e->extra->up.process_block_U2(e->extra->input, e->extra->input, audio[0] + offset,
                                          audio[1] + offset, 16);
        }
        if (e->model == Model::Twist)
        {
            const bool deactivated = !twist_lpg(e);
            if (e->oscdata.p[5].deactivated != deactivated)
            {
                e->oscdata.p[5].deactivated = deactivated;
                e->reset_pending = true;
            }
            // Rack uses the cable voltage directly for Twist's LPG, independent
            // of the Schmitt trigger used for phase reset. No cable means high.
            e->oscillator->setGate(e->rack_input_routing
                                       ? !e->trigger_connected || e->inputs[1] > 2.f
                                       : e->reset_high);
        }
        const auto requested_pitch =
            60.f + (e->params[0] - .5f + mod[0]) * 120.f +
            (e->inputs[0] + e->inputs[pitch_mod_input] + e->octave_shift) * 12.f;
        const auto pitch =
            e->model == Model::Alias || e->model == Model::String || separate_pitch_mod(e->model)
                ? requested_pitch
                : std::clamp(requested_pitch, 0.f, 148.f);
        if (e->reset_pending)
        {
            e->oscillator->init(pitch);
            e->reset_pending = false;
        }
        const float drift = std::clamp(e->params[8] + e->inputs[drift_cv_input] / 5.f, 0.f, 1.f);
        e->oscillator->process_block(pitch, drift, true);
        e->halfband.process_block_D2(e->oscillator->output, e->oscillator->outputR, block_size * 2,
                                     e->left, e->right);
        e->position = 0;
    }
    const float level = std::clamp(e->params[9] + e->inputs[level_cv_input] / 5.f, 0.f, 1.f);
    left = e->left[e->position] * 5.f * level;
    right = e->right[e->position] * 5.f * level;
    if (e->output_settings.dc_block)
    {
        // src/XTModule.h DCBlocker, after level and downsampling as in VCO.h.
        auto block = [&](float x, unsigned side) {
            float y = x - e->dc_x[side] + .9995f * e->dc_y[side];
            e->dc_x[side] = x;
            e->dc_y[side] = y;
            return y;
        };
        left = 5.f * block(e->left[e->position] * level, 0);
        right = 5.f * block(e->right[e->position] * level, 1);
    }
    if (e->extra)
        e->extra->input[e->position] = e->inputs[audio_input] / 5.f;
    ++e->position;
}
} // namespace surge_mm
