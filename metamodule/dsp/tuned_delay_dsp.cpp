// SPDX-License-Identifier: GPL-3.0-or-later
// Audio algorithm from src/DelayLineByFreq.h, Surge Synth Team 2019-2024.
#include "tuned_delay_dsp.h"
#include "filter_tuning.h"
#include <cstring>
#include <sst/utilities/globals.h>
#include <memory>
#include <sst/basic-blocks/dsp/SSESincDelayLine.h>
namespace surge_mm
{
namespace
{
using Line = sst::basic_blocks::dsp::SSESincDelayLine<16384>;
const auto &sinc()
{
    static const sst::basic_blocks::tables::SurgeSincTableProvider table;
    return table;
}
struct Voice
{
    Line left{sinc()}, right{sinc()};
};
} // namespace
struct TunedDelay
{
    std::array<std::unique_ptr<Voice>, 4> voices;
    float samplerate{48000};
    unsigned channels{1};
    TunedDelay()
    {
        (void)FilterTuning::note_to_pitch(0);
        for (auto &v : voices)
            v = std::make_unique<Voice>();
    }
};
TunedDelay *create_tuned_delay()
{
    return new TunedDelay;
}
void destroy_tuned_delay(TunedDelay *p)
{
    delete p;
}
void reset_tuned_delay(TunedDelay *p)
{
    for (auto &v : p->voices)
    {
        v->left.clear();
        v->right.clear();
    }
}
void tuned_delay_samplerate(TunedDelay *p, float rate)
{
    if (!std::isfinite(rate) || rate < 8000 || rate > 192000)
        return;
    p->samplerate = rate;
    reset_tuned_delay(p);
}
void process_tuned_delay(TunedDelay *p, const StereoPoly &audio, const PolyValues &pitch,
                         const PolyValues &correction, unsigned channels, StereoPoly &out)
{
    if (p->channels != channels)
    {
        reset_tuned_delay(p);
        p->channels = channels;
    }
    for (unsigned c = 0; c < channels; ++c)
    {
        // Preserve the original float/double evaluation and pitch LUT, not an
        // exp2 approximation: this determines the fractional sinc-table index.
        float note = pitch[c];
        auto frequency =
            FilterTuning::note_to_pitch_ignoring_tuning(note) * FilterTuning::MIDI_0_FREQ;
        float time = p->samplerate / frequency - correction[c];
        time = std::clamp(time, 12.f, 16384.f);
        auto &v = *p->voices[c];
        out[0][c] = v.left.read(time);
        out[1][c] = v.right.read(time);
        v.left.write(audio[0][c]);
        v.right.write(audio[1][c]);
    }
    for (unsigned c = channels; c < 4; ++c)
        out[0][c] = out[1][c] = 0;
}
} // namespace surge_mm
