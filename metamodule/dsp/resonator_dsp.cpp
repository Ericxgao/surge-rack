// SPDX-License-Identifier: GPL-3.0-or-later
#include "resonator_dsp.h"
#include "ResonatorKernel.h"
#include <memory>
namespace surge_mm
{
namespace
{
struct Voice
{
    resonator::ResonatorStorage global;
    resonator::ResonatorSettings settings;
    std::array<resonator::ResonatorValue, 12> values{};
    resonator::ResonatorKernel kernel{&global, &settings, values.data()};
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
struct Resonator
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    unsigned position{}, channels{1};
    Resonator()
    {
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
Resonator *create_resonator()
{
    return new Resonator;
}
void destroy_resonator(Resonator *p)
{
    delete p;
}
void reset_resonator(Resonator *p)
{
    p->position = 0;
    for (auto &v : p->voices)
        v->reset();
}
void resonator_samplerate(Resonator *p, float sr)
{
    if (!std::isfinite(sr) || sr < 8000 || sr > 192000)
        return;
    for (auto &v : p->voices)
        v->global.set_samplerate(sr);
    reset_resonator(p);
}
void process_resonator(Resonator *p, const std::array<float, 19> &params,
                       const std::array<PolyValues, 11> &cv, const StereoPoly &input,
                       unsigned channels, StereoPoly &out)
{
    if (channels != p->channels)
    {
        reset_resonator(p);
        p->channels = channels;
    }
    for (unsigned c = 0; c < channels; ++c)
        for (unsigned s = 0; s < 2; ++s)
            p->voices[c]->input[s][p->position] = input[s][c] * .2f;
    if (++p->position == 8)
    {
        p->position = 0;
        for (unsigned c = 0; c < channels; ++c)
        {
            auto &v = *p->voices[c];
            for (unsigned band = 0; band < 3; ++band)
            {
                unsigned id = 3 * band;
                bool extended = params[12 + band] > .5f;
                float lo = resonator_low[extended ? 0 : band],
                      hi = resonator_high[extended ? 2 : band];
                v.settings.p[id].val_min.f = lo;
                v.settings.p[id].val_max.f = hi;
                v.settings.p[id].extend_range = extended;
                v.values[id].f = std::clamp(lo + params[id] * (hi - lo) + 12 * cv[id][c], lo, hi);
                bool res_extended = params[15 + band] > .5f;
                v.settings.p[id + 1].extend_range = res_extended;
                v.values[id + 1].f =
                    std::clamp(params[id + 1] + cv[id + 1][c] * .2f, res_extended ? -4.f : 0.f,
                               res_extended ? 4.f : 1.f);
                v.values[id + 2].f = std::clamp(params[id + 2] + cv[id + 2][c] * .2f, 0.f, 1.f);
            }
            v.values[9].i = int(std::round(params[9] * 3));
            v.values[10].f = -48 + 96 * std::clamp(params[10] + cv[9][c] * .2f, 0.f, 1.f);
            v.values[11].f = std::clamp(params[11] + cv[10][c] * .2f, 0.f, 1.f);
            if (v.pending)
            {
                v.kernel.reset_native();
                v.pending = false;
            }
            for (unsigned s = 0; s < 2; ++s)
                std::copy_n(v.input[s], 8, v.processed[s]);
            v.kernel.process(v.processed[0], v.processed[1]);
        }
    }
    for (unsigned s = 0; s < 2; ++s)
        for (unsigned c = 0; c < 4; ++c)
            out[s][c] = c < channels ? p->voices[c]->processed[s][p->position] * 5 : 0;
}
} // namespace surge_mm
