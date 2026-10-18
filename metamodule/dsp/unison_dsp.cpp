// SPDX-License-Identifier: GPL-3.0-or-later
// DSP/routing adapted from src/UnisonHelper.h; original Surge kernels retained.
#include "unison_dsp.h"
#include "SurgeStorage.h"
#include "OscillatorCommonFunctions.h"
#include "BiquadFilter.h"
#include <memory>
namespace surge_mm
{
struct Unison
{
    SurgeStorage storage;
    Surge::Oscillator::DriftLFO drift[4];
    Surge::Oscillator::CharacterFilter<float> character[4];
    std::array<std::unique_ptr<BiquadFilter>, 4> low, high;
    bool low_on{}, high_on{};
    int previous_character{-1};
    unsigned position{}, previous_channels{1};
    std::array<PolyValues, 4> controls{};
    Surge::Oscillator::UnisonSetup<float> unison{3};
    Unison()
    {
        for (unsigned c = 0; c < 4; ++c)
        {
            drift[c].init(false);
            character[c].storage = &storage;
            low[c] = std::make_unique<BiquadFilter>(&storage);
            high[c] = std::make_unique<BiquadFilter>(&storage);
            low[c]->suspend();
            high[c]->suspend();
        }
    }
};
Unison *create_unison()
{
    return new Unison;
}
void destroy_unison(Unison *e)
{
    delete e;
}
void reset_unison(Unison *e)
{
    e->low_on = e->high_on = false;
    e->previous_character = -1;
    e->position = 0;
    e->previous_channels = 1;
    e->controls = {};
    e->unison = Surge::Oscillator::UnisonSetup<float>(3);
    for (unsigned c = 0; c < 4; ++c)
    {
        e->drift[c].init(false);
        e->low[c]->suspend();
        e->high[c]->suspend();
        e->character[c].priorX_L = e->character[c].priorX_R = 0;
        e->character[c].priorY_L = e->character[c].priorY_R = 0;
    }
}
void set_unison_samplerate(Unison *e, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    e->storage.setSamplerate(sr);
    e->low_on = e->high_on = false;
    e->position = 0;
    for (auto &character : e->character)
        character.init(character.type);
}
void process_unison(Unison *e, const std::array<float, 9> &p, const std::array<PolyValues, 9> &in,
                    const UnisonMap &map, bool stereo, bool audio_ready,
                    std::array<PolyValues, 6> &out, bool rack_compat)
{
    out = {};
    int character = int(std::round(p[6] * 2));
    if (character != e->previous_character)
    {
        for (auto &filter : e->character)
            filter.init(character);
        e->previous_character = character;
    }
    if (!rack_compat && map.channels != e->previous_channels)
    {
        for (unsigned lane = std::min(map.channels, e->previous_channels); lane < 4; ++lane)
        {
            e->low[lane]->suspend();
            e->high[lane]->suspend();
            e->drift[lane].init(false);
            e->character[lane].priorX_L = e->character[lane].priorX_R = 0;
            e->character[lane].priorY_L = e->character[lane].priorY_R = 0;
        }
        e->previous_channels = map.channels;
        e->position = 0;
        e->low_on = e->high_on = false;
    }
    if (e->position == 0)
    {
        bool lo = p[7] > .5f, hi = p[8] > .5f;
        for (unsigned c = 0; c < map.channels; ++c)
        {
            for (unsigned id = 0; id < 4; ++id)
            {
                float value = p[id] + in[5 + id][c] / 5.f;
                e->controls[id][c] = rack_compat ? value : std::clamp(value, 0.f, 1.f);
            }
            if (lo)
            {
                if (!e->low_on)
                    e->high[c]->suspend();
                e->high[c]->coeff_HP(e->high[c]->calc_omega((e->controls[2][c] * 130 - 60) / 12),
                                     .707);
                if (!e->low_on)
                    e->high[c]->coeff_instantize();
            }
            if (hi)
            {
                if (!e->high_on)
                    e->low[c]->suspend();
                e->low[c]->coeff_LP2B(e->low[c]->calc_omega((e->controls[3][c] * 130 - 60) / 12),
                                      .707);
                if (!e->high_on)
                    e->low[c]->coeff_instantize();
            }
            e->drift[c].next();
        }
        e->low_on = lo;
        e->high_on = hi;
    }
    if (e->unison.n_unison != int(map.voices))
        e->unison = Surge::Oscillator::UnisonSetup<float>(map.voices);
    if (map.fits)
        for (unsigned bank = 0; bank < map.banks; ++bank)
            for (unsigned lane = 0; lane < map.bank_channels[bank]; ++lane)
            {
                unsigned source = unsigned(map.source[bank][lane]),
                         voice = unsigned(map.voice[bank][lane]);
                float detune = e->controls[0][source] * (p[5] > .5f ? 1 : 1.f / 12);
                out[2 + bank][lane] = in[0][source] +
                                      e->controls[1][source] * e->drift[source].val() / 12 +
                                      e->unison.detune(voice) * detune;
                float value = in[1 + bank][lane];
                if (stereo)
                {
                    float left, right;
                    e->unison.attenuatedPanLaw(voice, left, right);
                    out[0][source] += value * left;
                    out[1][source] += value * right;
                }
                else
                    out[0][source] += value;
            }
    for (unsigned c = 0; c < map.channels; ++c)
    {
        e->character[c].process_block_stereo(&out[0][c], &out[1][c], 1);
        if (e->high_on)
            e->low[c]->process_sample(out[0][c], out[1][c], out[0][c], out[1][c]);
        if (e->low_on)
            e->high[c]->process_sample(out[0][c], out[1][c], out[0][c], out[1][c]);
        if (!audio_ready || !map.fits)
            out[0][c] = out[1][c] = 0;
    }
    e->position = (e->position + 1) % 8;
}
} // namespace surge_mm
