// SPDX-License-Identifier: GPL-3.0-or-later
// Embedded storage API for the selected Surge DSP. Ranges and extended mappings
// follow Surge Parameter.cpp; no desktop patch/database/filesystem is linked.
#pragma once
#include "globals.h"
#include "Wavetable.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <random>
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include "basic_dsp.h"
namespace Tunings
{
constexpr double MIDI_0_FREQ = 8.175798915643707;
}
constexpr int FIRipolI16_N = 8, FIRoffsetI16 = 4;
constexpr int FIRipol_N = 12;
constexpr int FIRipol_M = 256, FIRoffset = FIRipol_N >> 1;
union pdata
{
    int i;
    bool b;
    float f;
};
enum ctrltypes
{
    ct_countedset_percent_extendable,
    ct_pitch,
    ct_wt2window,
    ct_twist_engine,
    ct_twist_aux_mix,
    ct_percent_deactivatable,
    ct_percent_bipolar_w_dynamic_unipolar_formatting,
    ct_percent,
    ct_percent_bipolar,
    ct_fmratio_int,
    ct_fmratio,
    ct_freq_fm2_offset,
    ct_osc_feedback,
    ct_osc_feedback_negative,
    ct_freq_audible_fm3_extendable,
    ct_modern_trimix,
    ct_syncpitch,
    ct_oscspread,
    ct_osccount,
    ct_sineoscmode,
    ct_sinefmlegacy,
    ct_freq_audible_deactivatable_hp,
    ct_freq_audible_deactivatable_lp,
    ct_alias_wave,
    ct_alias_mask,
    ct_countedset_percent,
    ct_alias_bits,
    ct_stringosc_excitation_model,
    ct_percent_with_string_deform_hook,
    ct_percent_with_extend_to_bipolar_static_default,
    ct_oscspread_bipolar,
    ct_percent_bipolar_stringbal,
    ct_percent_bipolar_with_string_filter_hook
};
struct ParamUserData
{
    virtual ~ParamUserData() = default;
};
struct CountedSetUserData : ParamUserData
{
    virtual int getCountedSetSize() const = 0;
};
struct Parameter;
struct ParameterDynamicNameFunction
{
    virtual const char *getName(const Parameter *) const = 0;
    virtual ~ParameterDynamicNameFunction() = default;
};
struct Parameter
{
    pdata val{}, val_min{}, val_max{}, val_default{};
    int param_id_in_scene{}, deform_type{};
    bool extend_range{}, absolute{}, deactivated{}, integral{};
    ctrltypes ctrltype{ct_percent};
    const char *name{""};
    ParameterDynamicNameFunction *dynamicName{};
    void set_user_data(ParamUserData *)
    {
    }
    void set_extend_range(bool v)
    {
        extend_range = v;
    }
    void set_name(const char *n)
    {
        name = n;
    }
    void set_type(ctrltypes t)
    {
        ctrltype = t;
        integral = false;
        val_min.f = 0;
        val_max.f = 1;
        val_default.f = 0;
        switch (t)
        {
        case ct_twist_aux_mix:
        case ct_percent_bipolar_w_dynamic_unipolar_formatting:
        case ct_oscspread_bipolar:
        case ct_percent_bipolar_stringbal:
        case ct_percent_bipolar_with_string_filter_hook:
        case ct_percent_bipolar:
        case ct_modern_trimix:
        case ct_osc_feedback_negative:
            val_min.f = -1;
            break;
        case ct_pitch:
            val_min.f = -60;
            val_max.f = 60;
            break;
        case ct_wt2window:
            integral = true;
            val_min.i = 0;
            val_max.i = 8;
            break;
        case ct_alias_bits:
            val_min.f = 1;
            val_max.f = 8;
            val_default.f = 8;
            break;
        case ct_stringosc_excitation_model:
            integral = true;
            val_min.i = 0;
            val_max.i = 14;
            val_default.i = 0;
            break;
        case ct_twist_engine:
            integral = true;
            val_min.i = 0;
            val_max.i = 15;
            val_default.i = 0;
            break;
        case ct_alias_wave:
            integral = true;
            val_min.i = 0;
            val_max.i = 12;
            val_default.i = 0;
            break;
        case ct_fmratio:
            val_max.f = 32;
            val_default.f = 1;
            break;
        case ct_freq_fm2_offset:
            val_min.f = -10;
            val_max.f = 10;
            break;
        case ct_syncpitch:
            val_max.f = 60;
            break;
        case ct_oscspread:
            val_default.f = .1f;
            break;
        case ct_freq_audible_fm3_extendable:
        case ct_freq_audible_deactivatable_hp:
        case ct_freq_audible_deactivatable_lp:
            val_min.f = -60;
            val_max.f = 70;
            val_default.f = t == ct_freq_audible_deactivatable_hp   ? -60
                            : t == ct_freq_audible_deactivatable_lp ? 70
                                                                    : 3;
            break;
        case ct_fmratio_int:
        case ct_osccount:
        case ct_sineoscmode:
        case ct_sinefmlegacy:
            integral = true;
            val_min.i = (t == ct_fmratio_int || t == ct_osccount) ? 1 : 0;
            val_max.i = t == ct_fmratio_int   ? 32
                        : t == ct_osccount    ? 16
                        : t == ct_sineoscmode ? 27
                                              : 1;
            val_default.i = val_min.i;
            break;
        default:
            break;
        }
    }
    float get_extended(float f) const
    {
        if (!extend_range)
            return f;
        switch (ctrltype)
        {
        case ct_freq_fm2_offset:
            return 100.f * f;
        case ct_oscspread:
        case ct_oscspread_bipolar:
            return 12.f * f;
        case ct_osc_feedback:
        case ct_osc_feedback_negative:
            return 4.f * f;
        case ct_fmratio:
            return f > 16 ? (f - 16) * 31 / 16 + 1 : -((16 - f) * 31 / 16 + 1);
        default:
            return f;
        }
    }
    void set_normalized(float v)
    {
        v = std::clamp(v, 0.f, 1.f);
        if (integral)
            val.i = val_min.i + int(std::round(v * (val_max.i - val_min.i)));
        else
            val.f = val_min.f + v * (val_max.f - val_min.f);
    }
    float normalized() const
    {
        return integral ? float(val.i - val_min.i) / (val_max.i - val_min.i)
                        : (val.f - val_min.f) / (val_max.f - val_min.f);
    }
};
// Read-only table views keep the large buffers/pointer arrays shared across voices.
struct WavetableView
{
    int size{}, size_po2{}, flags{};
    unsigned n_tables{};
    float dt{};
    float *const (*TableF32WeakPointers)[max_subtables]{};
    short *const (*TableI16WeakPointers)[max_subtables]{};
    void bind(const Wavetable &w)
    {
        size = w.size;
        size_po2 = w.size_po2;
        flags = w.flags;
        n_tables = w.n_tables;
        dt = w.dt;
        TableF32WeakPointers = w.TableF32WeakPointers;
        TableI16WeakPointers = w.TableI16WeakPointers;
    }
};
struct OscillatorStorage : ParamUserData
{
    WavetableView wt;
    std::array<Parameter, 7> p{};
    Parameter retrigger;
    struct
    {
        int nData{};
        std::array<float, 16> data{};
    } extraConfig;
};
struct SurgeStorage
{
    float samplerate{48000}, samplerate_inv{1.f / 48000};
    double dsamplerate_inv{1.0 / 48000}, dsamplerate_os{96000}, dsamplerate_os_inv{1.0 / 96000};
    struct Patch
    {
        Parameter character;
    } patch;
    uint32_t random_state{0x13579bdf};
    WavetableView WindowWT;
    int16_t *sinctableI16{};
    float *sinctable{};
    float (*audio_in)[64]{};
    SurgeStorage()
    {
        static sst::basic_blocks::tables::SurgeSincTableProvider sinc;
        sinctable = sinc.sinctable;
        sinctableI16 = sinc.sinctableI16;
        patch.character.val.i = 1;
    }
    Patch &getPatch()
    {
        return patch;
    }
    void setSamplerate(float sr)
    {
        samplerate = sr;
        samplerate_inv = 1.f / sr;
        dsamplerate_inv = 1.0 / sr;
        dsamplerate_os = 2.0 * sr;
        dsamplerate_os_inv = .5 / sr;
    }
    uint32_t rand()
    {
        return rand_u32();
    }
    uint32_t rand_u32()
    {
        rand_01();
        return random_state;
    }
    float rand_01()
    {
        random_state ^= random_state << 13;
        random_state ^= random_state >> 17;
        random_state ^= random_state << 5;
        return (random_state >> 8) * (1.f / 16777216.f);
    }
    float note_to_pitch(float n) const
    {
        return std::exp2(n / 12.f);
    }
    float note_to_pitch_ignoring_tuning(float n) const
    {
        return note_to_pitch(n);
    }
    float note_to_pitch_inv(float n) const
    {
        return std::exp2(-n / 12.f);
    }
    float note_to_pitch_tuningctr(float n) const
    {
        return note_to_pitch(n);
    }
    float note_to_pitch_inv_tuningctr(float n) const
    {
        return note_to_pitch_inv(n);
    }
    float note_to_pitch_inv_ignoring_tuning(float n) const
    {
        return std::exp2(-n / 12.f);
    }
    float db_to_linear(float db) const
    {
        return std::pow(10.f, db / 20.f);
    }
};
