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
#include "reverb2_support.h"
namespace surge_mm::reverb2
{
template <bool Compact = true> class Reverb2Kernel : public Reverb2Base
{
    // These are set to make sure we have delay lengths up to 384k
    static constexpr int NUM_BLOCKS = 4, NUM_INPUT_ALLPASSES = 4, NUM_ALLPASSES_PER_BLOCK = 2,
                         MAX_ALLPASS_LEN = Compact ? 32768 : 16384 * 8, MAX_DELAY_LEN = 16384 * 8,
                         DELAY_LEN_MASK = MAX_DELAY_LEN - 1, DELAY_SUBSAMPLE_BITS = 8,
                         DELAY_SUBSAMPLE_RANGE = (1 << DELAY_SUBSAMPLE_BITS),
                         PREDELAY_BUFFER_SIZE =
                             (Compact ? 48000 * 8 * 3
                                      : 48000 * 8 * 4), // max sample rate is 48000 * 8 probably
        PREDELAY_BUFFER_SIZE_LIMIT = 48000 * 8 * 3;     // allow for one second of diffusion

    class allpass
    {
      public:
        allpass();
        void expand()
        {
            if constexpr (Compact)
                if (!_extension)
                    _extension = std::make_unique<float[]>(131072 - MAX_ALLPASS_LEN);
            _expanded = Compact;
        }
        std::unique_ptr<float[]> take_extension()
        {
            return std::move(_extension);
        }
        void keep_extension(std::unique_ptr<float[]> p)
        {
            _extension = std::move(p);
        }
        int storage_limit() const
        {
            return _expanded ? 131072 : MAX_ALLPASS_LEN;
        }
        float &sample_at(int index)
        {
            if constexpr (Compact)
                if (index >= MAX_ALLPASS_LEN)
                    return _extension[index - MAX_ALLPASS_LEN];
            return _data[index];
        }
        float process(float x, float coeff);
        void setLen(int len);

      public:
        int _len;
        int _k;
        float _data[MAX_ALLPASS_LEN];
        std::unique_ptr<float[]> _extension;
        bool _expanded{};
    };

    class delay
    {
      public:
        delay();
        float process(float x, int tap1, float &tap_out1, int tap2, float &tap_out2,
                      int modulation);
        void setLen(int len);

      public:
        int _len;
        int _k;
        float _data[MAX_DELAY_LEN];
    };

    class predelay
    {
      public:
        predelay()
        {
            memset(_data, 0, PREDELAY_BUFFER_SIZE * sizeof(float));
        }
        float process(float in, int tap)
        {
            k = (k + 1);
            if (k == PREDELAY_BUFFER_SIZE)
                k = 0;
            auto p = k - tap;
            while (p < 0)
                p += PREDELAY_BUFFER_SIZE;
            auto res = _data[p];
            _data[k] = in;
            return res;
        }

      public:
        int k = 0;
        float _data[PREDELAY_BUFFER_SIZE];
    };

    class onepole_filter
    {
      public:
        onepole_filter();
        float process_lowpass(float x, float c0);
        float process_highpass(float x, float c0);

      public:
        float a0;
    };

    lipol_ps_blocksz mix alignas(16), width alignas(16);

  public:
    Reverb2Kernel(Reverb2Storage *storage, Reverb2Settings *fxdata, Reverb2Value *pd);
    ~Reverb2Kernel();
    const char *get_effectname()
    {
        return "reverb2";
    }
    void init();
    void process(float *dataL, float *dataR);
    void suspend();
    void setvars(bool init);
    void calc_size(float scale);
    void init_ctrltypes();
    void init_default_values();
    const char *group_label(int id);
    int group_label_ypos(int id);
    int get_ringout_decay()
    {
        return ringout_time;
    }

    void reset_native()
    {
        // Reconstruct in the existing allocation: original init() only sets
        // lengths and leaves delay/filter/LFO/interpolator histories intact.
        auto *s = storage;
        auto *f = fxdata;
        auto *p = pd;

        // Preserve any import-time reservations through a native reset. The
        // audio callback must not free/reallocate the expanded storage.
        std::array<std::unique_ptr<float[]>,
                   NUM_INPUT_ALLPASSES + NUM_BLOCKS * NUM_ALLPASSES_PER_BLOCK>
            reserved;
        unsigned index = 0;
        for (auto &a : _input_allpass)
            reserved[index++] = a.take_extension();
        for (auto &block : _allpass)
            for (auto &a : block)
                reserved[index++] = a.take_extension();
        this->~Reverb2Kernel();
        new (this) Reverb2Kernel(s, f, p);
        index = 0;
        for (auto &a : _input_allpass)
            a.keep_extension(std::move(reserved[index++]));
        for (auto &block : _allpass)
            for (auto &a : block)
                a.keep_extension(std::move(reserved[index++]));
        init();
        update_rtime();
    }
    void expand_rack_storage()
    {
        for (auto &a : _input_allpass)
            a.expand();
        for (auto &block : _allpass)
            for (auto &a : block)
                a.expand();
    }
    enum rev2_params
    {
        rev2_predelay = 0,

        rev2_room_size,
        rev2_decay_time,
        rev2_diffusion,
        rev2_buildup,
        rev2_modulation,

        rev2_lf_damping,
        rev2_hf_damping,

        rev2_width,
        rev2_mix,

        rev2_num_params,
    };

  public:
    void update_rtime();
    int ringout_time{};
    allpass _input_allpass[NUM_INPUT_ALLPASSES];
    allpass _allpass[NUM_BLOCKS][NUM_ALLPASSES_PER_BLOCK];
    onepole_filter _hf_damper[NUM_BLOCKS];
    onepole_filter _lf_damper[NUM_BLOCKS];
    delay _delay[NUM_BLOCKS];
    predelay _predelay;
    int _tap_timeL[NUM_BLOCKS];
    int _tap_timeR[NUM_BLOCKS];
    float _tap_gainL[NUM_BLOCKS];
    float _tap_gainR[NUM_BLOCKS];
    float _state;
    lipol<float, true> _decay_multiply;
    lipol<float, true> _diffusion;
    lipol<float, true> _buildup;
    lipol<float, true> _hf_damp_coefficent;
    lipol<float, true> _lf_damp_coefficent;
    lipol<float, true> _modulation;

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc _lfo;
    float last_decay_time = -1.0;
};

inline const float db60 = powf(10.f, 0.05f * -60.f);
inline int msToSamples(float ms, float scale, float samplerate)
{
    float a = samplerate * ms * 0.001f;
    float b = a * scale;
    // these are clamped out above
    // assert( b < Reverb2Kernel::MAX_ALLPASS_LEN);
    // assert( b < Reverb2Kernel::MAX_DELAY_LEN);
    return (int)(b);
}
template <bool Compact> Reverb2Kernel<Compact>::allpass::allpass()
{
    _k = 0;
    _len = 1;
    memset(_data, 0, MAX_ALLPASS_LEN * sizeof(float));
}

template <bool Compact> void Reverb2Kernel<Compact>::allpass::setLen(int len)
{
    _len = std::clamp(len, 0, storage_limit() - 1);
}

template <bool Compact> float Reverb2Kernel<Compact>::allpass::process(float in, float coeff)
{
    _k++;
    if (_k >= _len)
        _k = 0;
    float delay_in = in - coeff * sample_at(_k);
    float result = sample_at(_k) + coeff * delay_in;
    sample_at(_k) = delay_in;
    return result;
}

template <bool Compact> Reverb2Kernel<Compact>::delay::delay()
{
    _k = 0;
    _len = 1;
    memset(_data, 0, MAX_DELAY_LEN * sizeof(float));
}

template <bool Compact> void Reverb2Kernel<Compact>::delay::setLen(int len)
{
    _len = std::clamp(len, 0, MAX_DELAY_LEN - 1);
}

template <bool Compact>
float Reverb2Kernel<Compact>::delay::process(float in, int tap1, float &tap_out1, int tap2,
                                             float &tap_out2, int modulation)
{
    _k = (_k + 1) & DELAY_LEN_MASK;

    tap_out1 = _data[(_k - tap1) & DELAY_LEN_MASK];
    tap_out2 = _data[(_k - tap2) & DELAY_LEN_MASK];

    int modulation_int = modulation >> DELAY_SUBSAMPLE_BITS;
    int modulation_frac1 = modulation & (DELAY_SUBSAMPLE_RANGE - 1);
    int modulation_frac2 = DELAY_SUBSAMPLE_RANGE - modulation_frac1;

    float d1 = _data[(_k - _len + modulation_int + 1) & DELAY_LEN_MASK];
    float d2 = _data[(_k - _len + modulation_int) & DELAY_LEN_MASK];
    const float multiplier = 1.f / (float)(DELAY_SUBSAMPLE_RANGE);

    float result = (d1 * (float)modulation_frac1 + d2 * (float)modulation_frac2) * multiplier;
    _data[_k] = in;

    return result;
}

template <bool Compact> Reverb2Kernel<Compact>::onepole_filter::onepole_filter()
{
    a0 = 0.f;
}

template <bool Compact>
float Reverb2Kernel<Compact>::onepole_filter::process_lowpass(float x, float c0)
{
    a0 = a0 * c0 + x * (1.f - c0);
    return a0;
}

template <bool Compact>
float Reverb2Kernel<Compact>::onepole_filter::process_highpass(float x, float c0)
{
    a0 = a0 * (1.f - c0) + x * c0;
    return x - a0;
}

template <bool Compact>
Reverb2Kernel<Compact>::Reverb2Kernel(Reverb2Storage *storage, Reverb2Settings *fxdata,
                                      Reverb2Value *pd)
    : Reverb2Base(storage, fxdata, pd)
{
    _state = 0.f;
}

template <bool Compact> Reverb2Kernel<Compact>::~Reverb2Kernel()
{
}

template <bool Compact> void Reverb2Kernel<Compact>::init()
{
    setvars(true);
}

template <bool Compact> void Reverb2Kernel<Compact>::calc_size(float scale)
{
    float m = scale;

    _tap_timeL[0] = msToSamples(80.3, m, storage->samplerate);
    _tap_timeL[1] = msToSamples(59.3, m, storage->samplerate);
    _tap_timeL[2] = msToSamples(97.7, m, storage->samplerate);
    _tap_timeL[3] = msToSamples(122.6, m, storage->samplerate);
    _tap_timeR[0] = msToSamples(35.5, m, storage->samplerate);
    _tap_timeR[1] = msToSamples(101.6, m, storage->samplerate);
    _tap_timeR[2] = msToSamples(73.9, m, storage->samplerate);
    _tap_timeR[3] = msToSamples(80.3, m, storage->samplerate);

    _input_allpass[0].setLen(msToSamples(4.76, m, storage->samplerate));
    _input_allpass[1].setLen(msToSamples(6.81, m, storage->samplerate));
    _input_allpass[2].setLen(msToSamples(10.13, m, storage->samplerate));
    _input_allpass[3].setLen(msToSamples(16.72, m, storage->samplerate));

    _allpass[0][0].setLen(msToSamples(38.2, m, storage->samplerate));
    _allpass[0][1].setLen(msToSamples(53.4, m, storage->samplerate));
    _delay[0].setLen(msToSamples(178.8, m, storage->samplerate));

    _allpass[1][0].setLen(msToSamples(44.0, m, storage->samplerate));
    _allpass[1][1].setLen(msToSamples(41, m, storage->samplerate));
    _delay[1].setLen(msToSamples(126.5, m, storage->samplerate));

    _allpass[2][0].setLen(msToSamples(48.3, m, storage->samplerate));
    _allpass[2][1].setLen(msToSamples(60.5, m, storage->samplerate));
    _delay[2].setLen(msToSamples(106.1, m, storage->samplerate));

    _allpass[3][0].setLen(msToSamples(38.9, m, storage->samplerate));
    _allpass[3][1].setLen(msToSamples(42.2, m, storage->samplerate));
    _delay[3].setLen(msToSamples(139.4, m, storage->samplerate));
}

template <bool Compact> void Reverb2Kernel<Compact>::setvars(bool init)
{
    // TODO: balance the gains from the calculated decay coefficient?
    _tap_gainL[0] = 1.5f / 4.f;
    _tap_gainL[1] = 1.2f / 4.f;
    _tap_gainL[2] = 1.0f / 4.f;
    _tap_gainL[3] = 0.8f / 4.f;
    _tap_gainR[0] = 1.5f / 4.f;
    _tap_gainR[1] = 1.2f / 4.f;
    _tap_gainR[2] = 1.0f / 4.f;
    _tap_gainR[3] = 0.8f / 4.f;

    calc_size(1.f);
}

template <bool Compact> void Reverb2Kernel<Compact>::update_rtime()
{
    auto ts = fxdata->p[rev2_predelay].temposync ? storage->temposyncratio_inv : 1.f;
    // * 2.f is to get the dB120 time
    auto pdlyt = std::max(0.1f, powf(2.f, *pd_float[rev2_predelay]) * ts) * 2.f;
    auto dcyt = std::max(1.0f, powf(2.f, *pd_float[rev2_decay_time])) * 2.f;
    float t = BLOCK_SIZE_INV * (storage->samplerate * (dcyt + pdlyt));

    ringout_time = (int)t;
}

template <bool Compact> void Reverb2Kernel<Compact>::process(float *dataL, float *dataR)
{
    float scale = powf(2.f, 1.f * *pd_float[rev2_room_size]);
    calc_size(scale);

    if (fabs(*pd_float[rev2_decay_time] - last_decay_time) > 0.001f)
        update_rtime();

    last_decay_time = *pd_float[rev2_decay_time];

    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];

    float loop_time_s = 0.5508 * scale;
    float decay = powf(db60, loop_time_s / (4.f * (powf(2.f, *pd_float[rev2_decay_time]))));

    _decay_multiply.newValue(decay);
    _diffusion.newValue(0.7f * *pd_float[rev2_diffusion]);
    _buildup.newValue(0.7f * *pd_float[rev2_buildup]);
    _hf_damp_coefficent.newValue(0.8 * *pd_float[rev2_hf_damping]);
    _lf_damp_coefficent.newValue(0.2 * *pd_float[rev2_lf_damping]);
    _modulation.newValue(*pd_float[rev2_modulation] * storage->samplerate * 0.001f * 5.f);

    width.set_target_smoothed(storage->db_to_linear(*pd_float[rev2_width]));
    mix.set_target_smoothed(*pd_float[rev2_mix]);

    _lfo.set_rate(2.0 * M_PI * powf(2, -2.f) * storage->dsamplerate_inv);

    int pdt =
        limit_range((int)(storage->samplerate * pow(2.f, *pd_float[rev2_predelay]) *
                          (fxdata->p[rev2_predelay].temposync ? storage->temposyncratio_inv : 1.f)),
                    1, PREDELAY_BUFFER_SIZE_LIMIT - 1);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float in = (dataL[k] + dataR[k]) * 0.5f;

        in = _predelay.process(in, pdt);

        in = _input_allpass[0].process(in, _diffusion.v);
        in = _input_allpass[1].process(in, _diffusion.v);
        in = _input_allpass[2].process(in, _diffusion.v);
        in = _input_allpass[3].process(in, _diffusion.v);
        float x = _state;

        float outL = 0.f;
        float outR = 0.f;

        float lfos[NUM_BLOCKS];
        lfos[0] = _lfo.r;
        lfos[1] = _lfo.i;
        lfos[2] = -_lfo.r;
        lfos[3] = -_lfo.i;

        auto hdc = limit_range(_hf_damp_coefficent.v, 0.01f, 0.99f);
        auto ldc = limit_range(_lf_damp_coefficent.v, 0.01f, 0.99f);
        for (int b = 0; b < NUM_BLOCKS; b++)
        {
            x = x + in;
            for (int c = 0; c < NUM_ALLPASSES_PER_BLOCK; c++)
            {
                x = _allpass[b][c].process(x, _buildup.v);
            }

            x = _hf_damper[b].process_lowpass(x, hdc);
            x = _lf_damper[b].process_highpass(x, ldc);

            int modulation = (int)(_modulation.v * lfos[b] * (float)DELAY_SUBSAMPLE_RANGE);
            float tap_outL = 0.f;
            float tap_outR = 0.f;
            x = _delay[b].process(x, _tap_timeL[b], tap_outL, _tap_timeR[b], tap_outR, modulation);
            outL += tap_outL * _tap_gainL[b];
            outR += tap_outR * _tap_gainR[b];

            x *= _decay_multiply.v;
        }

        wetL[k] = outL;
        wetR[k] = outR;
        _state = x;
        _decay_multiply.process();
        _diffusion.process();
        _buildup.process();
        _hf_damp_coefficent.process();
        _lfo.process();
        _modulation.process();
    }

    // scale width
    applyWidth(wetL, wetR, width);

    mix.fade_2_blocks_inplace(dataL, wetL, dataR, wetR, BLOCK_SIZE_QUAD);
}

template <bool Compact> void Reverb2Kernel<Compact>::suspend()
{
    init();
}

template <bool Compact> void Reverb2Kernel<Compact>::init_default_values()
{
    fxdata->p[rev2_predelay].val.f = -4.f;
    fxdata->p[rev2_decay_time].val.f = 0.75f;
    fxdata->p[rev2_mix].val.f = 0.33f;
    fxdata->p[rev2_width].val.f = 0.0f;
    fxdata->p[rev2_diffusion].val.f = 1.0f;
    fxdata->p[rev2_buildup].val.f = 1.0f;
    fxdata->p[rev2_modulation].val.f = 0.5f;
    fxdata->p[rev2_hf_damping].val.f = 0.2f;
    fxdata->p[rev2_lf_damping].val.f = 0.2f;
    fxdata->p[rev2_room_size].val.f = 0.f;
}
} // namespace surge_mm::reverb2
