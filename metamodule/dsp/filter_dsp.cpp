// SPDX-License-Identifier: GPL-3.0-or-later
// Routing/parameter behavior adapted from src/VCF.h, src/Waveshaper.h and
// src/XTModule.h. Filter and waveshaper kernels are the pinned SST implementations.
#include "filter_dsp.h"
#include "filter_tuning.h"
#include "SurgeStorage.h"
#include "BiquadFilter.h"
#include <sst/filters.h>
#include <sst/waveshapers.h>
#include <algorithm>
#include <cmath>
#include <memory>
namespace surge_mm
{
namespace sf = sst::filters;
namespace sw = sst::waveshapers;
namespace
{
constexpr unsigned block = 8;
constexpr unsigned delay_size = sf::utilities::MAX_FB_COMB + sf::utilities::SincTable::FIRipol_N;
unsigned discrete(float p, unsigned count)
{
    return unsigned(std::round(std::clamp(p, 0.f, 1.f) * (count - 1)));
}
float cube(float x)
{
    return x * x * x;
}
struct FilterVoiceState
{
    sf::QuadFilterUnitState state[2]{};
    sf::FilterCoefficientMaker<FilterTuning> coefficients[4];
    FilterTuning tuning;
    float delay[2][4][delay_size]{};
};
struct ShaperVoiceState
{
    sw::QuadWaveshaperState state[2]{};
    std::array<std::unique_ptr<BiquadFilter>, 4> low, high;
    __m128 prev_x[2]{}, prev_y[2]{};
};
// Some original shaping kernels construct lookup tables on first use. Populate
// them during module construction, never on the first audio-rate model change.
void warm_shapers()
{
    static const bool ready = [] {
        for (int t = 0; t < int(sw::WaveshaperType::n_ws_types); ++t)
        {
            auto type = sw::WaveshaperType(t);
            sw::QuadWaveshaperState state{};
            float initial[4]{};
            sw::initializeWaveshaperRegister(type, initial);
            for (int i = 0; i < 4; ++i)
                state.R[i] = _mm_set1_ps(initial[i]);
            state.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());
            if (auto fn = sw::GetQuadWaveshaper(type))
                fn(&state, _mm_setzero_ps(), _mm_set1_ps(1));
        }
        return true;
    }();
    (void)ready;
}
} // namespace
struct Filter
{
    FilterModule kind;
    SurgeStorage storage;
    std::unique_ptr<FilterVoiceState> filter;
    std::unique_ptr<ShaperVoiceState> shaper;
    sf::FilterUnitQFPtr filter_fn{};
    sw::QuadWaveshaperPtr shaper_fn{};
    int model{-1}, subtype{-1};
    float max_cutoff_note{12.f * std::log2(48000.f * .499f / 440.f)};
    unsigned position{};
    std::array<unsigned, 2> channels{};
    bool low_on{}, high_on{}, dc_on{};
    std::array<PolyValues, 3> current{}, delta{};
    Filter(FilterModule k) : kind(k)
    {
        if (k == FilterModule::VCF)
        {
            filter = std::make_unique<FilterVoiceState>();
            for (auto &c : filter->coefficients)
                c.setSampleRateAndBlockSize(48000, block);
        }
        else
        {
            warm_shapers();
            shaper = std::make_unique<ShaperVoiceState>();
            for (unsigned c = 0; c < 4; ++c)
            {
                shaper->low[c] = std::make_unique<BiquadFilter>(&storage);
                shaper->high[c] = std::make_unique<BiquadFilter>(&storage);
            }
        }
        for (auto &v : current)
            v.fill(1);
    }
};
unsigned filter_model_count(FilterModule m)
{
    return m == FilterModule::VCF ? sf::num_filter_types : unsigned(sw::WaveshaperType::n_ws_types);
}
unsigned filter_subtype_count(unsigned t)
{
    return t < unsigned(sf::num_filter_types) ? std::max(1, sf::fut_subcount[t]) : 1;
}
const char *filter_model_name(FilterModule m, unsigned t)
{
    if (t >= filter_model_count(m))
        return "Invalid";
    return m == FilterModule::VCF ? sf::filter_type_names[t] : sw::wst_names[t];
}
std::string filter_subtype_name(unsigned t, unsigned i)
{
    if (i >= filter_subtype_count(t))
        return "Invalid";
    switch (t)
    {
    case sf::fut_none:
    case sf::fut_SNH:
    case sf::fut_apf:
        return "Default";
    case sf::fut_lpmoog:
    case sf::fut_diode:
        return sf::fut_ldr_subtypes[i];
    case sf::fut_notch12:
    case sf::fut_notch24:
        return sf::fut_notch_subtypes[i];
    case sf::fut_comb_pos:
    case sf::fut_comb_neg:
        return sf::fut_comb_subtypes[i];
    case sf::fut_vintageladder:
        return sf::fut_vintageladder_subtypes[i];
    case sf::fut_obxd_2pole_lp:
    case sf::fut_obxd_2pole_hp:
    case sf::fut_obxd_2pole_bp:
    case sf::fut_obxd_2pole_n:
        return sf::fut_obxd_2p_subtypes[i];
    case sf::fut_obxd_4pole:
        return sf::fut_obxd_4p_subtypes[i];
    case sf::fut_k35_lp:
    case sf::fut_k35_hp:
        return sf::fut_k35_subtypes[i];
    case sf::fut_cutoffwarp_lp:
    case sf::fut_cutoffwarp_hp:
    case sf::fut_cutoffwarp_n:
    case sf::fut_cutoffwarp_bp:
    case sf::fut_cutoffwarp_ap:
    case sf::fut_resonancewarp_lp:
    case sf::fut_resonancewarp_hp:
    case sf::fut_resonancewarp_n:
    case sf::fut_resonancewarp_bp:
    case sf::fut_resonancewarp_ap:
        return std::string(sf::fut_nlf_subtypes[i & 3]) + " " + sf::fut_nlf_saturators[i >> 2];
    case sf::fut_tripole:
        return std::string(sf::fut_tripole_subtypes[i & 3]) + " " +
               sf::fut_tripole_output_stage[i >> 2];
    default:
        return sf::fut_def_subtypes[i];
    }
}
std::array<float, 9> filter_defaults(FilterModule m)
{
    if (m == FilterModule::VCF)
        return {.4f,      std::sqrt(.5f),
                .5f,      1,
                .5f,      float(sf::fut_obxd_4pole) / (sf::num_filter_types - 1),
                3.f / 11, 0,
                0};
    return {
        .5f, .5f,
        .5f, 0,
        1,   float(sw::WaveshaperType::wst_ojd) / (unsigned(sw::WaveshaperType::n_ws_types) - 1),
        0,   0,
        1};
}
Filter *create_filter(FilterModule m)
{
    auto *e = new Filter(m);
    reset_filter(e);
    return e;
}
void destroy_filter(Filter *e)
{
    delete e;
}
void reset_filter(Filter *e)
{
    e->position = 0;
    e->model = e->subtype = -1;
    e->low_on = e->high_on = e->dc_on = false;
    if (e->filter)
    {
        auto &f = *e->filter;
        for (auto &c : f.coefficients)
            c.Reset();
        for (auto &side : f.delay)
            for (auto &lane : side)
                std::fill(std::begin(lane), std::end(lane), 0.f);
        for (unsigned s = 0; s < 2; ++s)
        {
            f.state[s] = {};
            for (unsigned c = 0; c < 4; ++c)
            {
                f.state[s].DB[c] = f.delay[s][c];
                f.state[s].active[c] = ~0u;
            }
        }
    }
    if (e->shaper)
    {
        for (unsigned s = 0; s < 2; ++s)
        {
            e->shaper->prev_x[s] = e->shaper->prev_y[s] = _mm_setzero_ps();
        }
        for (unsigned c = 0; c < 4; ++c)
        {
            e->shaper->low[c]->suspend();
            e->shaper->high[c]->suspend();
        }
    }
    for (auto &v : e->current)
        v.fill(1);
    for (auto &v : e->delta)
        v.fill(0);
}
void filter_samplerate(Filter *e, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    e->storage.setSamplerate(sr);
    e->max_cutoff_note = 12.f * std::log2(sr * .499f / 440.f);
    if (e->filter)
    {
        e->filter->tuning.set_samplerate(sr);
        for (auto &c : e->filter->coefficients)
            c.setSampleRateAndBlockSize(sr, block);
    }
    reset_filter(e);
}
void process_filter(Filter *e, const std::array<float, 9> &p, const std::array<PolyValues, 5> &cv,
                    const StereoPoly &audio, std::array<unsigned, 2> channels, StereoPoly &out)
{
    const bool vcf = e->kind == FilterModule::VCF;
    int type = discrete(p[5], filter_model_count(e->kind));
    int subtype = vcf ? std::min(discrete(p[6], 12), filter_subtype_count(type) - 1) : 0;
    if (e->channels != channels)
    {
        reset_filter(e);
        e->channels = channels;
    }
    std::array<PolyValues, 5> values{};
    for (unsigned i = 0; i < 5; ++i)
        for (unsigned c = 0; c < 4; ++c)
            values[i][c] = std::clamp(p[i] + cv[i][c] * (vcf && i == 0 ? .1f : .2f), 0.f, 1.f);
    if (e->position == 0)
    {
        if (type != e->model || subtype != e->subtype)
        {
            reset_filter(e);
            e->model = type;
            e->subtype = subtype;
            if (vcf)
                e->filter_fn =
                    sf::GetQFPtrFilterUnit(sf::FilterType(type), sf::FilterSubType(subtype));
            else
            {
                e->shaper_fn = sw::GetQuadWaveshaper(sw::WaveshaperType(type));
                float initial[4]{};
                sw::initializeWaveshaperRegister(sw::WaveshaperType(type), initial);
                for (auto &st : e->shaper->state)
                {
                    for (unsigned i = 0; i < 4; ++i)
                        st.R[i] = _mm_set1_ps(initial[i]);
                    st.init = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());
                }
            }
        }
        if (vcf)
        {
            auto &f = *e->filter;
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &cm = f.coefficients[c];
                cm.updateCoefficients(f.state[channels[0] > c ? 0 : 1], c);
                // Lattice coefficient conversion is singular at exactly Nyquist.
                // Keep the requested cutoff inside the representable frequency band.
                cm.MakeCoeffs(std::min((values[0][c] * 10 - 4) * 12 - 9, e->max_cutoff_note),
                              values[1][c], sf::FilterType(type), sf::FilterSubType(subtype),
                              &f.tuning, false);
                for (unsigned s = 0; s < 2; ++s)
                {
                    cm.updateState(f.state[s], c);
                    f.state[s].active[c] = channels[s] > c ? ~0u : 0;
                }
                const float targets[]{cube(values[2][c] * 2), cube(values[4][c] * 2), values[3][c]};
                for (unsigned i = 0; i < 3; ++i)
                    e->delta[i][c] = (targets[i] - e->current[i][c]) / block;
            }
        }
        else
        {
            bool low = p[6] > .5f, high = p[7] > .5f, dc = p[8] > .5f;
            for (unsigned c = 0; c < 4; ++c)
            {
                auto &lo = *e->shaper->high[c];
                auto &hi = *e->shaper->low[c];
                if (low)
                {
                    if (!e->low_on)
                        lo.suspend();
                    lo.coeff_HP(lo.calc_omega((values[3][c] * 130 - 60) / 12), .707);
                    if (!e->low_on)
                        lo.coeff_instantize();
                }
                if (high)
                {
                    if (!e->high_on)
                        hi.suspend();
                    hi.coeff_LP2B(hi.calc_omega((values[4][c] * 130 - 60) / 12), .707);
                    if (!e->high_on)
                        hi.coeff_instantize();
                }
            }
            if (dc && !e->dc_on)
                for (unsigned s = 0; s < 2; ++s)
                    e->shaper->prev_x[s] = e->shaper->prev_y[s] = _mm_setzero_ps();
            e->low_on = low;
            e->high_on = high;
            e->dc_on = dc;
        }
    }
    if (vcf)
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned c = 0; c < 4; ++c)
                e->current[i][c] += e->delta[i][c];
    for (unsigned s = 0; s < 2; ++s)
    {
        alignas(16) float in[4]{}, drive[4]{}, gain[4]{}, mix[4]{}, bias[4]{};
        std::fill(std::begin(drive), std::end(drive), 1.f);
        for (unsigned c = 0; c < channels[s]; ++c)
        {
            in[c] = audio[s][c] * .2f;
            drive[c] = vcf ? e->current[0][c] : e->storage.db_to_linear(values[0][c] * 48 - 24);
            gain[c] = vcf ? e->current[1][c] : cube(values[2][c] * 2);
            mix[c] = e->current[2][c];
            bias[c] = values[1][c] * 2 - 1;
        }
        auto input = _mm_load_ps(in);
        __m128 result;
        if (vcf)
        {
            auto wet = _mm_mul_ps(input, _mm_load_ps(drive));
            if (e->filter_fn)
                wet = e->filter_fn(&e->filter->state[s], wet);
            auto m = _mm_load_ps(mix);
            result = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(wet, _mm_load_ps(gain)), m),
                                _mm_mul_ps(input, _mm_sub_ps(_mm_set1_ps(1), m)));
        }
        else
        {
            result = _mm_add_ps(input, _mm_load_ps(bias));
            if (e->shaper_fn)
                result = e->shaper_fn(&e->shaper->state[s], result, _mm_load_ps(drive));
            result = _mm_mul_ps(result, _mm_load_ps(gain));
            if (e->dc_on)
            {
                auto x = result;
                result = _mm_add_ps(_mm_sub_ps(x, e->shaper->prev_x[s]),
                                    _mm_mul_ps(_mm_set1_ps(.9995f), e->shaper->prev_y[s]));
                e->shaper->prev_x[s] = x;
                e->shaper->prev_y[s] = result;
            }
        }
        alignas(16) float samples[4];
        _mm_store_ps(samples, _mm_mul_ps(result, _mm_set1_ps(5)));
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels[s] ? samples[c] : 0;
    }
    if (!vcf)
        for (unsigned c = 0; c < 4; ++c)
        {
            auto &l = out[0][c];
            auto &r = out[1][c];
            if (e->high_on)
                e->shaper->low[c]->process_sample(l, r, l, r);
            if (e->low_on)
                e->shaper->high[c]->process_sample(l, r, l, r);
            if (c >= channels[0])
                l = 0;
            if (c >= channels[1])
                r = 0;
        }
    e->position = (e->position + 1) % block;
}
} // namespace surge_mm
