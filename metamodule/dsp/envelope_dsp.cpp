// SPDX-License-Identifier: GPL-3.0-or-later
// Link routing adapted from src/QuadAD.h, copyright Surge Synth Team contributors.
#include "envelope_dsp.h"
#include "modulation_clock.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include "sst/basic-blocks/modulators/ADAREnvelope.h"
namespace surge_mm
{
struct QuadAD
{
    using Envelope = sst::basic_blocks::modulators::ADAREnvelope<EnvelopeClock, 8>;
    EnvelopeClock clock;
    std::array<std::array<std::unique_ptr<Envelope>, 4>, 4> env;
    Edge trigger[4][4]{}, link_trigger[4][4]{};
    std::array<PolyValues, 4> link_eoc{};
    unsigned block_pos{};
    struct RackState
    {
        struct Trigger
        {
            unsigned state{2};
            bool process(float v)
            {
                if (state == 0 && v >= 1)
                {
                    state = 1;
                    return true;
                }
                if (state == 1 && v <= 0)
                    state = 0;
                else if (state == 2)
                {
                    if (v >= 1)
                        state = 1;
                    else if (v <= 0)
                        state = 0;
                }
                return false;
            }
        };
        Trigger external[4][4], linked[4][4];
        bool gated[4][4]{}, incoming[4]{}, sum[4]{}, any_link{};
        std::array<int, 4> push{-1, -1, -1, -1}, last{-1, -1, -1, -1};
        unsigned co_trigger[4]{}, channels[4]{1, 1, 1, 1};
        float accumulated[4][4]{}, eoc[4][4]{}, times[8][4]{};
    } rack;
    QuadAD()
    {
        for (auto &stage : env)
            for (auto &voice : stage)
            {
                voice = std::make_unique<Envelope>(&clock);
                voice->immediatelySilence();
            }
    }
};
QuadAD *create_quad_ad()
{
    return new QuadAD;
}
void destroy_quad_ad(QuadAD *engine)
{
    delete engine;
}
void reset_quad_ad(QuadAD *e)
{
    e->block_pos = 0;
    e->rack = {};
    e->link_eoc = {};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < 4; ++c)
        {
            *e->env[i][c] = QuadAD::Envelope(&e->clock);
            e->env[i][c]->immediatelySilence();
            e->trigger[i][c] = {};
            e->link_trigger[i][c] = {};
        }
}
void set_quad_ad_samplerate(QuadAD *engine, float sr)
{
    if (std::isfinite(sr) && sr >= 8000 && sr <= 192000)
        engine->clock.set_samplerate(sr);
}
static void process_rack_quad_ad(QuadAD *e, const std::array<float, 33> &p,
                                 const std::array<PolyValues, 12> &in,
                                 const std::array<unsigned, 4> &counts,
                                 std::array<PolyValues, 4> &out, std::array<unsigned, 4> &channels)
{
    auto &r = e->rack;
    if (e->block_pos == 0)
    {
        bool recalc = false;
        unsigned maximum = 1;
        for (unsigned i = 0; i < 4; ++i)
        {
            r.sum[i] = p[28 + (i + 3) % 4] > .5f;
            int mode = int(std::round(p[24 + i] * 2 - 1));
            recalc |= mode != r.last[i];
            r.last[i] = mode;
            r.channels[i] = std::clamp(counts[i], 1u, 4u);
            maximum = std::max(maximum, r.channels[i]);
        }
        if (recalc)
        {
            r.push.fill(-1);
            std::fill_n(r.incoming, 4, false);
            std::fill_n(r.co_trigger, 4, 0);
            r.any_link = false;
            for (unsigned i = 0; i < 4; ++i)
            {
                r.any_link |= r.last[i] != 0;
                if (r.last[i] == 1)
                {
                    r.push[i] = (i + 1) % 4;
                    r.incoming[r.push[i]] = true;
                }
                if (r.last[i] == -1)
                {
                    unsigned start = i;
                    for (unsigned n = 1; n < 4; ++n)
                    {
                        unsigned previous = (i + 4 - n) % 4;
                        if (r.last[previous] != 1)
                            break;
                        start = previous;
                    }
                    r.push[i] = start;
                    r.incoming[i] = r.incoming[start] = true;
                }
            }
            for (unsigned i = 0; i < 4; ++i)
            {
                for (unsigned n = 0; n < 4; ++n)
                {
                    unsigned pos = (i + n) % 4;
                    if (r.push[pos] != int((pos + 1) % 4))
                        break;
                    r.co_trigger[i] |= 1u << r.push[pos];
                }
                for (unsigned n = 1; n < 4; ++n)
                {
                    unsigned pos = (i + 4 - n) % 4;
                    if (r.push[pos] != int((pos + 1) % 4))
                        break;
                    r.co_trigger[i] |= 1u << pos;
                }
                if (r.co_trigger[i])
                    r.co_trigger[i] |= 1u << i;
            }
        }
        if (r.any_link)
            std::fill_n(r.channels, 4, maximum);
        for (auto &v : r.eoc)
            std::fill_n(v, 4, 0);
        for (unsigned i = 0; i < 4; ++i)
            if (r.push[i] >= 0)
                for (unsigned c = 0; c < 4; ++c)
                    r.eoc[r.push[i]][c] += e->env[i][c]->eoc_output;
        for (unsigned i = 0; i < 8; ++i)
            for (unsigned c = 0; c < 4; ++c)
                r.times[i][c] = (p[i] * ad_time_span - 8) + (in[4 + i][c] / 5.f) * ad_time_span;
    }
    out = {};
    for (unsigned i = 0; i < 4; ++i)
    {
        channels[i] = 1;
        if (!(counts[i] || r.incoming[i] || r.sum[i] || r.push[i] >= 0))
            continue;
        channels[i] = r.channels[i];
        int attack_shape = int(p[12 + i] * 2), decay_shape = int(p[16 + i] * 2);
        for (unsigned c = 0; c < channels[i]; ++c)
        {
            auto &env = *e->env[i][c];
            float voltage = in[i][c];
            bool external = r.external[i][c].process(voltage),
                 linked = r.linked[i][c].process(r.incoming[i] && r.eoc[i][c] > 0 ? 10.f : 0.f);
            float nuked = 0;
            if (external)
                for (unsigned other = 0; other < 4; ++other)
                    if (other != i && (r.co_trigger[i] & (1u << other)))
                    {
                        nuked = std::max(nuked, e->env[other][c]->output);
                        e->env[other][c]->immediatelySilence();
                    }
            if (external || linked)
            {
                r.gated[i][c] = true;
                env.attackFrom(p[32] > .5f ? 0 : std::max(env.output, nuked), attack_shape,
                               p[8 + i] < .5f, p[20 + i] > .5f);
            }
            if (r.gated[i][c] && voltage < 1)
                r.gated[i][c] = false;
            env.processScaledAD(r.times[i][c], r.times[4 + i][c], attack_shape, decay_shape,
                                r.gated[i][c]);
            float value = env.output * 10;
            if (r.sum[i])
                value += r.accumulated[(i + 3) % 4][c];
            out[i][c] = r.accumulated[i][c] = value;
        }
    }
    e->block_pos = (e->block_pos + 1) % 8;
}
void process_quad_ad(QuadAD *engine, const std::array<float, 33> &p,
                     const std::array<PolyValues, 12> &inputs,
                     const std::array<unsigned, 4> &trigger_channels,
                     std::array<PolyValues, 4> &outputs, std::array<unsigned, 4> &channels,
                     bool rack_compat)
{
    if (rack_compat)
    {
        process_rack_quad_ad(engine, p, inputs, trigger_channels, outputs, channels);
        return;
    }
    std::array<int, 4> mode{}, push{-1, -1, -1, -1};
    std::array<bool, 4> incoming{}, active{};
    std::array<unsigned, 4> co_trigger{};
    bool any_link = false;
    unsigned max_channels = 1;
    for (unsigned i = 0; i < 4; ++i)
    {
        mode[i] = int(std::round(p[24 + i] * 2)) - 1;
        any_link |= mode[i] != 0;
        channels[i] = std::clamp(trigger_channels[i], 1u, 4u);
        max_channels = std::max(max_channels, channels[i]);
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        if (mode[i] == 1)
        {
            push[i] = (i + 1) % 4;
            incoming[push[i]] = true;
        }
        if (mode[i] == -1)
        {
            unsigned start = i;
            for (unsigned n = 1; n < 4; ++n)
            {
                unsigned prev = (i + 4 - n) % 4;
                if (mode[prev] != 1)
                    break;
                start = prev;
            }
            push[i] = start;
            incoming[i] = incoming[start] = true;
        }
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        for (unsigned n = 0; n < 4; ++n)
        {
            unsigned pos = (i + n) % 4;
            if (push[pos] != int((pos + 1) % 4))
                break;
            co_trigger[i] |= 1u << push[pos];
        }
        for (unsigned n = 1; n < 4; ++n)
        {
            unsigned pos = (i + 4 - n) % 4;
            if (push[pos] != int((pos + 1) % 4))
                break;
            co_trigger[i] |= 1u << pos;
        }
        if (co_trigger[i])
            co_trigger[i] |= 1u << i;
        if (any_link)
            channels[i] = max_channels;
        active[i] = trigger_channels[i] || incoming[i] || push[i] >= 0 || p[28 + (i + 3) % 4] > .5f;
    }
    // Rack snapshots EOC routing at the Surge eight-sample block boundary.
    if (engine->block_pos == 0)
    {
        engine->link_eoc = {};
        for (unsigned i = 0; i < 4; ++i)
            if (push[i] >= 0)
                for (unsigned lane = 0; lane < channels[i]; ++lane)
                    engine->link_eoc[push[i]][lane] += engine->env[i][lane]->eoc_output;
    }
    std::array<PolyValues, 4> raw{};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            auto &env = *engine->env[i][lane];
            if (!active[i] || lane >= channels[i])
            {
                env.immediatelySilence();
                engine->trigger[i][lane] = {};
                engine->link_trigger[i][lane] = {};
                continue;
            }
            const bool external = engine->trigger[i][lane].process(inputs[i][lane]);
            const bool linked = engine->link_trigger[i][lane].process(
                incoming[i] && engine->link_eoc[i][lane] > 0 ? 10 : 0);
            float previous = env.output;
            if (external)
                for (unsigned other = 0; other < 4; ++other)
                    if (other != i && (co_trigger[i] & (1u << other)))
                    {
                        previous = std::max(previous, engine->env[other][lane]->output);
                        engine->env[other][lane]->immediatelySilence();
                        raw[other][lane] = 0;
                    }
            int ashape = int(std::round(p[12 + i] * 2)), dshape = int(std::round(p[16 + i] * 2));
            if (external || linked)
                env.attackFrom(p[32] > .5f ? 0 : previous, ashape, p[8 + i] < .5f, p[20 + i] > .5f);
            float attack =
                std::clamp(p[i] + inputs[4 + i][lane] / 5.f, 0.f, 1.f) * ad_time_span - 8;
            float decay =
                std::clamp(p[4 + i] + inputs[8 + i][lane] / 5.f, 0.f, 1.f) * ad_time_span - 8;
            env.processScaledAD(attack, decay, ashape, dshape, inputs[i][lane] >= 1);
            raw[i][lane] = env.output * 10;
        }
    outputs = {};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned lane = 0; lane < channels[i]; ++lane)
        {
            unsigned source = i, visited = 0;
            // Each source contributes once, even when all four sum links are on.
            // The Rack accumulator otherwise feeds back its previous sample forever.
            while (!(visited & (1u << source)))
            {
                visited |= 1u << source;
                outputs[i][lane] += raw[source][lane];
                source = (source + 3) % 4;
                if (p[28 + source] <= .5f)
                    break;
            }
        }
    engine->block_pos = (engine->block_pos + 1) % 8;
}
} // namespace surge_mm
