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

#pragma once
#include "nimbus_support.h"
namespace surge_mm::nimbus
{
template <bool LatchTrigger = true> class NimbusKernel : public NimbusBase
{
  public:
    bool nativeTriggerLatch{LatchTrigger};
    void reset_native()
    {
        std::memset(block_mem, 0, 118784);
        std::memset(block_ccm, 0, 65536 - 128);
        std::memset(processor, 0, sizeof(*processor));
        processor->Init(block_mem, 118784, block_ccm, 65536 - 128);
        if (surgeSR_to_euroSR)
            src_reset(surgeSR_to_euroSR);
        if (euroSR_to_surgeSR)
            src_reset(euroSR_to_surgeSR);
        numStubs = 0;
        nimbusTrigger = false;
        old_nmb_mode = 0;
        std::memset(stub_input, 0, sizeof(stub_input));
        std::memset(L, 0, sizeof(L));
        std::memset(R, 0, sizeof(R));
        mix = lipol_ps_blocksz{};
        init();
    }
    bool ready() const
    {
        return surgeSR_to_euroSR && euroSR_to_surgeSR;
    }
    enum nmb_params
    {
        nmb_mode,
        nmb_quality,

        nmb_position,
        nmb_size,
        nmb_pitch,
        nmb_density,
        nmb_texture,
        nmb_spread,

        nmb_freeze,
        nmb_feedback,

        nmb_reverb,
        nmb_mix,

        nmb_num_params,
    };

  public:
    lipol_ps_blocksz mix alignas(16);
    float L alignas(16)[BLOCK_SIZE], R alignas(16)[BLOCK_SIZE];

  public:
    NimbusKernel(NimbusStorage *storage, NimbusSettings *fxdata, NimbusValue *pd);
    ~NimbusKernel();
    const char *get_effectname()
    {
        return "Nimbus";
    }
    void init();
    void process(float *dataL, float *dataR);
    void suspend();
    void setvars(bool init);
    void init_ctrltypes();
    void init_default_values();

    const char *group_label(int id);
    int group_label_ypos(int id);

    int get_ringout_decay()
    {
        return -1;
    }

    // Only used by rack
    void setNimbusTrigger(bool b)
    {
        nimbusTrigger = nativeTriggerLatch ? nimbusTrigger || b : b;
    }

  public:
    uint8_t *block_mem, *block_ccm;
    clouds::GranularProcessor *processor;
    static constexpr int processor_sr = 32000;
    static constexpr float processor_sr_inv = 1.f / 32000;
    int old_nmb_mode = 0;
    bool nimbusTrigger{false};

    SRC_STATE_tag *surgeSR_to_euroSR, *euroSR_to_surgeSR;

    static constexpr int raw_out_sz = BLOCK_SIZE_OS << 5; // power of 2 pls
    float resampled_output[raw_out_sz][2];                // at sr
    size_t resampReadPtr = 0, resampWritePtr = 1;         // see comment in init

    static constexpr int nimbusprocess_blocksize = 8;
    float stub_input[2][nimbusprocess_blocksize]; // This is the extra sample we have around
    size_t numStubs{0};
    int consumed = 0, created = 0;
    bool builtBuffer{false};
};

template <bool LatchTrigger>
NimbusKernel<LatchTrigger>::NimbusKernel(NimbusStorage *storage, NimbusSettings *fxdata,
                                         NimbusValue *pd)
    : NimbusBase(storage, fxdata, pd)
{
    const int memLen = 118784;
    const int ccmLen = 65536 - 128;
    block_mem = new uint8_t[memLen]();
    block_ccm = new uint8_t[ccmLen]();
    processor = new clouds::GranularProcessor();
#if EURORACK_CLOUDS_IS_SUPERPARASITES
#else
    memset(processor, 0, sizeof(*processor));
#endif

    processor->Init(block_mem, memLen, block_ccm, ccmLen);
    mix.set_blocksize(BLOCK_SIZE);

    int error;
    surgeSR_to_euroSR = src_new(SRC_SINC_FASTEST, 2, &error);
    if (error != 0)
    {
        surgeSR_to_euroSR = nullptr;
    }

    euroSR_to_surgeSR = src_new(SRC_SINC_FASTEST, 2, &error);
    if (error != 0)
    {
        euroSR_to_surgeSR = nullptr;
    }
}

template <bool LatchTrigger> NimbusKernel<LatchTrigger>::~NimbusKernel()
{
    delete[] block_mem;
    delete[] block_ccm;
    delete processor;

    if (surgeSR_to_euroSR)
        surgeSR_to_euroSR = src_delete(surgeSR_to_euroSR);
    if (euroSR_to_surgeSR)
        euroSR_to_surgeSR = src_delete(euroSR_to_surgeSR);
}

template <bool LatchTrigger> void NimbusKernel<LatchTrigger>::init()
{
    mix.set_target(1.f);
    mix.instantize();

    memset(resampled_output, 0, raw_out_sz * 2 * sizeof(float));

    consumed = 0;
    created = 0;
    builtBuffer = false;
    resampReadPtr = 0;
    resampWritePtr = 1; // why 1? well while we are stalling we want to output 0 so write 1 ahead
}

template <bool LatchTrigger> void NimbusKernel<LatchTrigger>::setvars(bool init)
{
}

template <bool LatchTrigger> void NimbusKernel<LatchTrigger>::process(float *dataL, float *dataR)
{
    setvars(false);

    if (!surgeSR_to_euroSR || !euroSR_to_surgeSR)
        return;

    /* Resample Temp Buffers */
    float resample_this[BLOCK_SIZE << 3][2];
    float resample_into[BLOCK_SIZE << 3][2];

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        resample_this[i][0] = dataL[i];
        resample_this[i][1] = dataR[i];
    }

    SRC_DATA sdata;
    sdata.end_of_input = 0;
    sdata.src_ratio = processor_sr * storage->samplerate_inv;
    sdata.data_in = &(resample_this[0][0]);
    sdata.data_out = &(resample_into[0][0]);
    sdata.input_frames = BLOCK_SIZE;
    sdata.output_frames = (BLOCK_SIZE << 3);
    auto res = src_process(surgeSR_to_euroSR, &sdata);
    consumed += sdata.input_frames_used;

    if (sdata.output_frames_gen)
    {
        clouds::ShortFrame input[BLOCK_SIZE << 3];
        clouds::ShortFrame output[BLOCK_SIZE << 3];

        int frames_to_go = sdata.output_frames_gen;
        int outpos = 0;

        auto modeInt = *pd_int[nmb_mode];
        // Just make sure we are safe if we swap between superparasites and not
#if EURORACK_CLOUDS_IS_SUPERPARASITES
        modeInt = std::clamp(modeInt, 0, 7);
#else
        modeInt = std::clamp(modeInt, 0, 3);
#endif
        processor->set_playback_mode(
            (clouds::PlaybackMode)((int)clouds::PLAYBACK_MODE_GRANULAR + modeInt));
        processor->set_quality(*pd_int[nmb_quality]);

        int consume_ptr = 0;
        while (frames_to_go + numStubs >= nimbusprocess_blocksize)
        {
            int sp = 0;
            while (numStubs > 0)
            {
                input[sp].l = (short)(clamp1bp(stub_input[0][sp]) * 32767.0f);
                input[sp].r = (short)(clamp1bp(stub_input[1][sp]) * 32767.0f);
                sp++;
                numStubs--;
            }

            for (int i = sp; i < nimbusprocess_blocksize; ++i)
            {
                input[i].l = (short)(clamp1bp(resample_into[consume_ptr][0]) * 32767.0f);
                input[i].r = (short)(clamp1bp(resample_into[consume_ptr][1]) * 32767.0f);
                consume_ptr++;
            }

            int inputSz = nimbusprocess_blocksize; // sdata.output_frames_gen + sp;

            auto parm = processor->mutable_parameters();

            float den_val, tex_val;

            den_val = (*pd_float[nmb_density] + 1.f) * 0.5;
            tex_val = (*pd_float[nmb_texture] + 1.f) * 0.5;

            parm->position = clamp01(*pd_float[nmb_position]);
            parm->size = clamp01(*pd_float[nmb_size]);
            parm->density = clamp01(den_val);
            parm->texture = clamp01(tex_val);
            parm->pitch = limit_range(*pd_float[nmb_pitch], -48.f, 48.f);
            parm->stereo_spread = clamp01(*pd_float[nmb_spread]);
            parm->feedback = clamp01(*pd_float[nmb_feedback]);
            parm->freeze = *pd_float[nmb_freeze] > 0.5;
            parm->reverb = clamp01(*pd_float[nmb_reverb]);
            parm->dry_wet = 1.f;

#if EURORACK_CLOUDS_IS_SUPERPARASITES
            parm->capture = nimbusTrigger;
#else
            parm->trigger = nimbusTrigger;
            if (nativeTriggerLatch)
                nimbusTrigger = false; // this is an external granulating source. Skip it
#endif
            parm->gate = parm->freeze; // This is the CV for the freeze button

            processor->Prepare();
            processor->Process(input, output, inputSz);

            for (int i = 0; i < inputSz; ++i)
            {
                resample_this[outpos + i][0] = output[i].l / 32767.0f;
                resample_this[outpos + i][1] = output[i].r / 32767.0f;
            }
            outpos += inputSz;
            frames_to_go -= (nimbusprocess_blocksize - sp);
        }

        if (frames_to_go > 0)
        {
            int startSub = numStubs;
            int addStub = frames_to_go;
            numStubs += frames_to_go;

            for (int i = 0; i < addStub; ++i)
            {
                stub_input[0][i + startSub] = resample_into[consume_ptr][0];
                stub_input[1][i + startSub] = resample_into[consume_ptr][1];
                consume_ptr++;
            }
        }

        if (outpos > 0)
        {
            SRC_DATA odata;
            odata.end_of_input = 0;
            odata.src_ratio = processor_sr_inv * storage->samplerate;
            odata.data_in = &(resample_this[0][0]);
            odata.data_out = &(resample_into[0][0]);
            odata.input_frames = outpos;
            odata.output_frames = BLOCK_SIZE << 3;
            auto reso = src_process(euroSR_to_surgeSR, &odata);
            if (!builtBuffer)
                created += odata.output_frames_gen;

            size_t w = resampWritePtr;
            for (int i = 0; i < odata.output_frames_gen; ++i)
            {
                resampled_output[w][0] = resample_into[i][0];
                resampled_output[w][1] = resample_into[i][1];

                w = (w + 1U) & (raw_out_sz - 1U);
            }
            resampWritePtr = w;
        }
    }

    // If you hit this you need to adjust this gapping ratio probably.
    static_assert(BLOCK_SIZE >= nimbusprocess_blocksize);
    int ratio = std::max((int)std::ceil(processor_sr_inv * storage->samplerate) - 2, 0);
    bool rpi = (created) > (BLOCK_SIZE * (1 + ratio) + 8); // leave some buffer
    if (rpi)
        builtBuffer = true;

    size_t rp = resampReadPtr;
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        L[i] = resampled_output[rp][0];
        R[i] = resampled_output[rp][1];
        rp = (rp + rpi) & (raw_out_sz - 1);
    }
    resampReadPtr = rp;

    mix.set_target_smoothed(clamp01(*pd_float[nmb_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

template <bool LatchTrigger> void NimbusKernel<LatchTrigger>::suspend()
{
    init();
}

template <bool LatchTrigger> void NimbusKernel<LatchTrigger>::init_default_values()
{
    fxdata->p[nmb_mode].val.i = 0;
    fxdata->p[nmb_quality].val.i = 0;
    fxdata->p[nmb_position].val.f = 0.f;
    fxdata->p[nmb_size].val.f = 0.5;
    fxdata->p[nmb_density].val.f = 0.f;
    fxdata->p[nmb_texture].val.f = 0.f;
    fxdata->p[nmb_pitch].val.f = 0.f;
    fxdata->p[nmb_freeze].val.f = 0.f;
    fxdata->p[nmb_feedback].val.f = 0.f;
    fxdata->p[nmb_reverb].val.f = 0.f;
    fxdata->p[nmb_spread].val.f = 0.f;
    fxdata->p[nmb_mix].val.f = 0.5;
}
} // namespace surge_mm::nimbus
