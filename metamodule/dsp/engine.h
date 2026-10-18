// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "table_data.h"
namespace surge_mm
{
enum class Model
{
    FM2,
    FM3,
    Modern,
    Sine,
    Classic,
    SHNoise,
    Alias,
    String,
    Twist,
    Wavetable,
    Window
};
constexpr bool fm_oscillator(Model model)
{
    return model == Model::FM2 || model == Model::FM3;
}
constexpr bool standard_oscillator(Model model)
{
    return fm_oscillator(model) || model == Model::Classic || model == Model::Modern ||
           model == Model::Sine || model == Model::SHNoise;
}
constexpr bool table_oscillator(Model model)
{
    return model == Model::Wavetable || model == Model::Window;
}
constexpr unsigned table_extra_param(Model model)
{
    return model == Model::Window ? 16 : 14;
}
constexpr bool separate_pitch_mod(Model model)
{
    return standard_oscillator(model) || model == Model::Twist || table_oscillator(model);
}
constexpr unsigned standard_option_count(Model model)
{
    return model == Model::Classic || model == Model::FM2   ? 5
           : model == Model::Modern                         ? 8
           : model == Model::Sine                           ? 9
           : model == Model::SHNoise || model == Model::FM3 ? 7
                                                            : 0;
}
constexpr int block_size = 8;
constexpr int base_parameter_count = 10; // pitch, seven engine controls, drift, level
constexpr int modulation_sources = 4;
constexpr int modulation_targets = 8; // pitch and seven engine controls
constexpr int parameter_count = base_parameter_count + modulation_sources * modulation_targets;
constexpr int modulation_input_start = 9; // after pitch, reset, seven direct CVs
constexpr int drift_cv_input = modulation_input_start + modulation_sources;
constexpr int level_cv_input = drift_cv_input + 1;
constexpr int audio_input = level_cv_input + 1;
constexpr int pitch_mod_input = audio_input + 1;
constexpr int input_count = pitch_mod_input + 1;
constexpr int alias_option_count = 21;
constexpr int modulation_param(int destination, int source)
{
    return base_parameter_count + destination * modulation_sources + source;
}
struct Engine;
struct OscillatorOutputSettings
{
    unsigned halfband_order{6};
    bool steep{true}, dc_block{};
    bool operator==(const OscillatorOutputSettings &other) const
    {
        return halfband_order == other.halfband_order && steep == other.steep &&
               dc_block == other.dc_block;
    }
};
// Restore Rack's saved output stage without exposing its JSON to the DSP.
bool configure_output(Engine *, OscillatorOutputSettings);
void set_octave_shift(Engine *, float normalized);
float get_octave_shift(const Engine *);
// Octave, detune range/type, character, phase; Modern adds multi shape/sub/sync.
void set_standard_option(Engine *, int id, float normalized);
float get_standard_option(const Engine *, int id);
Engine *create(Model model);
void destroy(Engine *engine);
void sample_rate(Engine *engine, float sr);
void set_param(Engine *engine, int id, float normalized);
float get_param(const Engine *engine, int id);
const char *param_name(const Engine *engine, int id);
constexpr int twist_option_count = 4; // LPG Auto/Off/On, Aux stereo, character, phase reset
void set_twist_option(Engine *, int id, float normalized);
float get_twist_option(const Engine *, int id);
void set_trigger_connected(Engine *, bool connected);
void set_rack_input_routing(Engine *, bool enabled);
// Caller retains the table handle; switching/resetting happens outside rendering.
void set_wavetable(Engine *, const TableHandle &);
constexpr int wavetable_option_count =
    8; // interpolation, detune range/type, phase, low/high cut enable, octave, character
void set_wavetable_option(Engine *, int id, float normalized);
float get_wavetable_option(const Engine *, int id);
constexpr int string_option_count = 9;
void set_string_option(Engine *, int id, float normalized);
float get_string_option(const Engine *, int id);
void set_alias_option(Engine *, int id, float normalized);
float get_alias_option(const Engine *, int id);
void input(Engine *engine, int id, float voltage);
void process(Engine *engine, float &left, float &right);
} // namespace surge_mm
