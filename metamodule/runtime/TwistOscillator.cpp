/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "TwistOscillator.h"

#define TEST
#ifndef _MSC_VER
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
#endif
#include "plaits/dsp/voice.h"

std::string twist_engine_name(int i)
{
    switch (i)
    {
    case 0:
        return "Waveforms";
    case 1:
        return "Waveshaper";
    case 2:
        return "2-Operator FM";
    case 3:
        return "Formant/PD";
    case 4:
        return "Harmonic";
    case 5:
        return "Wavetable";
    case 6:
        return "Chords";
    case 7:
        return "Vowels/Speech";
    case 8:
        return "Granular Cloud";
    case 9:
        return "Filtered Noise";
    case 10:
        return "Particle Noise";
    case 11:
        return "Inharmonic String";
    case 12:
        return "Modal Resonator";
    case 13:
        return "Analog Kick";
    case 14:
        return "Analog Snare";
    case 15:
        return "Analog Hi-Hat";
    }
    return "Error " + std::to_string(i);
}

TwistOscillator::TwistOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                 pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy), charFilt(storage)
{
#if SAMPLERATE_LANCZOS
    lancRes = std::make_unique<sst::basic_blocks::dsp::LanczosResampler<BLOCK_SIZE>>(
        48000, storage->dsamplerate_os);
    srcstate = nullptr;
#else
    int error;
    srcstate = src_new(SRC_SINC_FASTEST, 2, &error);
    // srcstate = src_new(SRC_LINEAR, 2, &error);
    if (error != 0)
    {
        srcstate = nullptr;
    }
#endif
    voice = std::make_unique<plaits::Voice>();
    shared_buffer = new char[16384];
    alloc = std::make_unique<stmlib::BufferAllocator>(shared_buffer, 16384);
    patch = std::make_unique<plaits::Patch>();
    mod = std::make_unique<plaits::Modulations>();

    fmdownsamplestate = nullptr;
}

float TwistOscillator::tuningAwarePitch(float pitch)
{
    return std::max(pitch, -24.f);
}

void TwistOscillator::init(float pitch, bool is_display, bool nonzero_drift)
{
    auto &res = *lancRes;
    res.sro = storage->dsamplerate_os;
    res.dPhaseO = res.sri / res.sro;
    res.phaseI = res.phaseO = 0;
    res.wp = 0;
    memset(res.input, 0, sizeof(res.input));
    voice->~Voice();
    memset(static_cast<void *>(voice.get()), 0, sizeof(plaits::Voice));
    new (voice.get()) plaits::Voice();
    memset(shared_buffer, 0, 16384);
    voice->Init(alloc.get());

    charFilt.init(storage->getPatch().character.val.i);

    float tpitch = tuningAwarePitch(pitch);
    memset((void *)patch.get(), 0, sizeof(plaits::Patch));
    memset((void *)mod.get(), 0, sizeof(plaits::Modulations));

    driftLFO.init(nonzero_drift);

    // Lets run forward a cycle
    int throwaway = 0;
    double cycleInSamples = std::max(1.0, 1.0 / pitch_to_dphase(tpitch));

    while (cycleInSamples < 10)
        cycleInSamples *= 2;

    if (!(oscdata->retrigger.val.b || is_display))
    {
        cycleInSamples *= (1.0 + storage->rand_01());
    }

    memset(fmlagbuffer, 0, (BLOCK_SIZE_OS << 1) * sizeof(float));
    fmrp = 0;
    fmwp = (int)(BLOCK_SIZE_OS * 48000 * storage->dsamplerate_os_inv);

    process_block_internal<false, true>(pitch, 0, false, 0, std::ceil(cycleInSamples));
}
TwistOscillator::~TwistOscillator()
{
    if (shared_buffer)
        delete[] shared_buffer;
}

template <bool FM, bool throwaway>
void TwistOscillator::process_block_internal(float pitch, float drift, bool stereo, float FMdepth,
                                             int throwawayBlocks)
{
#if SAMPLERATE_SRC
    if (!srcstate)
        return;
#endif

    if (FM && !fmdownsamplestate)
        return;

    pitch = tuningAwarePitch(pitch);

    auto driftv = driftLFO.next();
    patch->note = pitch + drift * driftv;
    patch->engine = oscdata->p[twist_engine].val.i;

    harm.newValue(limit_range(fvbp(twist_harmonics), -1.f, 1.f));
    timb.newValue(limit_range(fvbp(twist_timbre), -1.f, 1.f));
    morph.newValue(limit_range(fvbp(twist_morph), -1.f, 1.f));
    lpgcol.newValue(limit_range(fv(twist_lpg_response), 0.f, 1.f));
    lpgdec.newValue(limit_range(fv(twist_lpg_decay), 0.f, 1.f));
    auxmix.newValue(limit_range(fvbp(twist_aux_mix), -1.f, 1.f));

    bool lpgIsOn = !oscdata->p[twist_lpg_response].deactivated;

    // The LPG in surge 1.1 is wrong because it doesn't use the correct vlock
    // size. This code allows you to optionally correct that while retaining legacy.
    // See #6760 for more.
    constexpr int max_subblock = 12; // This is the internal block size PLAITS uses

    // This setting is *incorrect* but retains legacy surge XT 1.1 behavior
    int subblock = (lpgIsOn || FM) ? 1 : 4; // retain surge legacy

    // This setting allows us to correct it in VCV Rack.
    if (lpgIsOn && useCorrectLPGBlockSize)
        subblock = 12;
#if SAMPLERATE_SRC
    float src_in[subblock][2];
    float src_out[BLOCK_SIZE_OS][2];

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = dsamplerate_os / 48000.0;
#endif

    float normFMdepth = 0;

    int required_blocks = throwaway ? throwawayBlocks : BLOCK_SIZE_OS;

#if SAMPLERATE_SRC
    for (int i = 0; i < carrover_size; ++i)
    {
        if (oscdata->p[twist_aux_mix].extend_range)
        {
            output[i] = auxmix.v * carryover[i][1] + (1.0 - auxmix.v) * carryover[i][0];
            outputR[i] = auxmix.v * carryover[i][0] + (1.0 - auxmix.v) * carryover[i][1];
        }
        else
        {
            output[i] = auxmix.v * carryover[i][1] + (1.0 - auxmix.v) * carryover[i][0];
            outputR[i] = output[i];
        }
    }
    int total_generated = carrover_size;
    carrover_size = 0;
#else
    int total_generated =
        required_blocks - lancRes->inputsRequiredToGenerateOutputs(required_blocks);
#endif

    if (lpgIsOn)
    {
        mod->trigger = gate ? 1.0 : 0.0;
        mod->trigger_patched = true;
    }

    while (total_generated < required_blocks)
    {
        plaits::Voice::Frame poutput[max_subblock];
        patch->harmonics = harm.v;
        patch->timbre = timb.v;
        patch->morph = morph.v;
        patch->decay = lpgdec.v;
        patch->lpg_colour = lpgcol.v;

        harm.process();
        timb.process();
        morph.process();
        lpgdec.process();
        lpgcol.process();
#if SAMPLERATE_SRC
        auxmix.process();
#endif

        if (FM)
        {
            mod->frequency_patched = true;
            mod->frequency = 137 * fmlagbuffer[fmrp]; // this is in 'notes'
            fmrp = (fmrp + 1) & ((BLOCK_SIZE_OS << 1) - 1);
            patch->frequency_modulation_amount = normFMdepth;
        }
        else
        {
            mod->frequency_patched = false;
            patch->frequency_modulation_amount = 0;
        }

        voice->Render(*patch, *mod, poutput, subblock);

#if SAMPLERATE_LANCZOS
        for (int i = 0; i < subblock; ++i)
        {
            lancRes->push(poutput[i].out / 32768.f, poutput[i].aux / 32768.f);
        }
        total_generated =
            required_blocks - lancRes->inputsRequiredToGenerateOutputs(required_blocks);
#else
        for (int i = 0; i < subblock; ++i)
        {
            src_in[i][0] = poutput[i].out / 32768.f;
            src_in[i][1] = poutput[i].aux / 32768.f;
        }
        sdata.data_in = &(src_in[0][0]);
        sdata.data_out = &(src_out[0][0]);
        sdata.input_frames = subblock;
        sdata.output_frames = BLOCK_SIZE_OS;
        auto res = src_process(srcstate, &sdata);
        // FIXME - check res

        for (int i = 0; i < sdata.output_frames_gen; ++i)
        {
            if (i + total_generated >= required_blocks)
            {
                carryover[carrover_size][0] = src_out[i][0];
                carryover[carrover_size][1] = src_out[i][1];
                carrover_size++;
            }
            else if (!throwaway)
            {
                if (oscdata->p[twist_aux_mix].extend_range)
                {
                    output[total_generated + i] =
                        auxmix.v * src_out[i][1] + (1 - auxmix.v) * src_out[i][0];
                    outputR[total_generated + i] =
                        auxmix.v * src_out[i][0] + (1 - auxmix.v) * src_out[i][1];
                }
                else
                {
                    output[total_generated + i] =
                        auxmix.v * src_out[i][1] + (1 - auxmix.v) * src_out[i][0];
                    outputR[total_generated + i] = output[total_generated + i];
                }
            }
        }
        total_generated += sdata.output_frames_gen;
        if (sdata.input_frames_used != subblock)
        {
            // FIXME
            std::cout << "DEAL " << std::endl;
        }
#endif
    }

#if SAMPLERATE_LANCZOS
    if (throwaway)
    {
        lancRes->advanceReadPointer(required_blocks);
    }
    else
    {
        alignas(16) float tL[BLOCK_SIZE_OS], tR[BLOCK_SIZE_OS];
        lancRes->populateNextBlockSizeOS(tL, tR);

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            if (oscdata->p[twist_aux_mix].extend_range)
            {
                output[i] = auxmix.v * tL[i] + (1 - auxmix.v) * tR[i];
                outputR[i] = auxmix.v * tR[i] + (1 - auxmix.v) * tL[i];
            }
            else
            {
                output[i] = auxmix.v * tR[i] + (1 - auxmix.v) * tL[i];
                outputR[i] = output[i];
            }
            auxmix.process();
        }
    }
    lancRes->renormalizePhases();
#endif

    if (!throwaway && charFilt.doFilter)
    {
        if (charFilt.doFilter)
        {
            if (stereo)
            {
                charFilt.process_block_stereo(output, outputR, BLOCK_SIZE_OS);
            }
            else
            {
                charFilt.process_block(output, BLOCK_SIZE_OS);
            }
        }
    }
}

void TwistOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth)
{
    TwistOscillator::process_block_internal<false>(pitch, drift, stereo, 0);
}

void TwistOscillator::init_ctrltypes()
{
    oscdata->p[twist_engine].set_name("Engine");
    oscdata->p[twist_engine].set_type(ct_twist_engine);

    oscdata->p[twist_harmonics].set_name("Harmonics");
    oscdata->p[twist_harmonics].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);

    oscdata->p[twist_timbre].set_name("Timbre");
    oscdata->p[twist_timbre].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);

    oscdata->p[twist_morph].set_name("Morph");
    oscdata->p[twist_morph].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);

    oscdata->p[twist_aux_mix].set_name("Aux Mix");
    oscdata->p[twist_aux_mix].set_type(ct_twist_aux_mix);

    oscdata->p[twist_lpg_response].set_name("LPG Response");
    oscdata->p[twist_lpg_response].set_type(ct_percent_deactivatable);

    oscdata->p[twist_lpg_decay].set_name("LPG Decay");
    oscdata->p[twist_lpg_decay].set_type(ct_percent);
}

void TwistOscillator::init_default_values()
{
    oscdata->p[twist_engine].val.i = 0.f;
    oscdata->p[twist_harmonics].val.f = 0.f;
    oscdata->p[twist_timbre].val.f = 0.f;
    oscdata->p[twist_morph].val.f = 0.f;
    oscdata->p[twist_aux_mix].val.f = -1.f;
    oscdata->p[twist_aux_mix].set_extend_range(false);
    oscdata->p[twist_lpg_response].val.f = 0.f;
    oscdata->p[twist_lpg_response].deactivated = true;
    oscdata->p[twist_lpg_decay].val.f = 0.f;
}
