// SPDX-License-Identifier: GPL-3.0-or-later
// File interpretation follows SurgeStorage.cpp / WAVFileSupport.cpp. Original
// Wavetable::BuildWT and MipMapWT perform all sample conversion and filtering.
#include "table_data_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace surge_mm
{
extern const uint8_t default_table[], window_table[];
extern const size_t default_table_size, window_table_size;
namespace
{
uint16_t u16(const uint8_t *p)
{
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
uint32_t u32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put16(uint8_t *p, uint16_t v)
{
    p[0] = v;
    p[1] = v >> 8;
}
void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = v >> (8 * i);
}
bool tag(const uint8_t *p, const char *s)
{
    return std::memcmp(p, s, 4) == 0;
}
bool dimensions(unsigned samples, unsigned frames)
{
    return samples >= 2 && samples <= 4096 && !(samples & (samples - 1)) && frames >= 1 &&
           frames <= 512;
}
TableHandle build(unsigned samples, unsigned frames, unsigned flags, const uint8_t *payload,
                  size_t available, unsigned channels, bool append_silence, std::string &error)
{
    auto fail = [&](const char *s) -> TableHandle {
        error = s;
        return {};
    };
    if (!dimensions(samples, frames) || (append_silence && frames > 509))
        return fail(
            "Table must have 1-512 frames of 2-4096 power-of-two samples (sample playback reserves 3 silent frames)");
    if (flags & ~15u)
        return fail("Unsupported wavetable flags");
    if ((flags & 8) && !(flags & 4))
        return fail("16-bit range flag requires integer samples");
    const size_t stride = flags & 4 ? 2 : 4;
    auto result = std::make_shared<TableData>();
    result->bytes.resize(12 + size_t(samples) * (frames + (append_silence ? 3 : 0)) * stride, 0);
    auto *b = result->bytes.data();
    std::memcpy(b, "vawt", 4);
    put32(b + 4, samples);
    put16(b + 8, frames);
    put16(b + 10, flags);
    // Left channel only for stereo WAV, as in Surge. Short legacy WT files are
    // zero-padded (the factory Oneshot/Pulse.wt is one such file).
    for (size_t i = 0; i < size_t(samples) * frames; ++i)
    {
        const auto offset = i * channels * stride;
        if (offset < available)
            std::memcpy(b + 12 + i * stride, payload + offset,
                        std::min(stride, available - offset));
        if (!(flags & 4))
        {
            uint32_t bits = u32(b + 12 + i * 4);
            float sample;
            std::memcpy(&sample, &bits, 4);
            if (!std::isfinite(sample) || std::abs(sample) >= 131071.f)
                return fail("Invalid floating-point wavetable sample");
        }
    }
    wt_header header;
    std::memcpy(&header, b, 12);
    if (!result->table.BuildWT(b + 12, header, append_silence))
        return fail("Unable to allocate wavetable buffers");
    // Canonical serialization includes appended silence, so reload must not append again.
    put16(b + 8, result->table.n_tables);
    return result;
}
} // namespace
TableHandle load_table(const uint8_t *bytes, size_t size, std::string &error, int wav_frame_size)
{
    error.clear();
    auto fail = [&](const char *s) -> TableHandle {
        error = s;
        return {};
    };
    if (!bytes || size < 12)
        return fail("Wavetable file is shorter than its header");
    if (tag(bytes, "vawt"))
    {
        if (size == 12)
            return fail("Wavetable has no sample data");
        return build(u32(bytes + 4), u16(bytes + 8), u16(bytes + 10), bytes + 12, size - 12, 1,
                     false, error);
    }
    if (!tag(bytes, "RIFF") || !tag(bytes + 8, "WAVE"))
        return fail("Expected a Surge WT or RIFF/WAVE file");
    unsigned format = 0, channels = 0, bits = 0, align = 0;
    const uint8_t *data = nullptr;
    size_t data_size = 0;
    unsigned clm = 0, srge = 0, smpl = 0, cue = 0, srgo = 0;
    // Inspect actual bounded chunks. Some Surge-exported files overstate the RIFF
    // length by two bytes; the size field never authorizes an out-of-bounds read.
    for (size_t pos = 12; pos < size;)
    {
        if (size - pos < 8)
            return fail("Incomplete WAV chunk header");
        auto *head = bytes + pos;
        size_t count = u32(head + 4);
        pos += 8;
        if (count > size - pos)
            return fail("Incomplete WAV chunk payload");
        const auto *p = bytes + pos;
        if (tag(head, "fmt "))
        {
            if (count < 16)
                return fail("Incomplete WAV format");
            format = u16(p);
            channels = u16(p + 2);
            align = u16(p + 12);
            bits = u16(p + 14);
        }
        else if (tag(head, "data"))
        {
            data = p;
            data_size = count;
        }
        else if (tag(head, "clm "))
        {
            if (count >= 7 && tag(p + 3, "2048"))
                clm = 2048;
        }
        else if (tag(head, "uhWT"))
            clm = 2048;
        else if (tag(head, "srge") || tag(head, "srgo"))
        {
            if (count < 8)
                return fail("Incomplete Surge WAV metadata");
            if (tag(head, "srge"))
                srge = u32(p + 4);
            else
                srgo = u32(p + 4);
            if (u32(p + 4) == 0)
                return fail("Zero WAV frame size");
        }
        else if (tag(head, "smpl"))
        {
            if (count < 36)
                return fail("Incomplete WAV sampler metadata");
            const auto loops = u32(p + 28);
            if (!loops)
                smpl = 2048;
            else
            {
                if (count < 60)
                    return fail("Incomplete WAV sample loop");
                const auto start = u32(p + 44), end = u32(p + 48);
                if (end < start || end - start == UINT32_MAX)
                    return fail("Invalid WAV sample loop");
                smpl = end - start + 1;
            }
        }
        else if (tag(head, "cue "))
        {
            if (count < 4 || u32(p) > (count - 4) / 24)
                return fail("Incomplete WAV cue metadata");
            const auto n = u32(p);
            if (n >= 2)
            {
                auto previous = u32(p + 24), next = u32(p + 48);
                bool regular = next > previous;
                const auto length = next - previous;
                previous = next;
                for (unsigned i = 2; i < n; ++i)
                {
                    next = u32(p + 24 + i * 24);
                    regular &= next > previous && next - previous == length;
                    previous = next;
                }
                if (regular)
                    cue = length;
            }
        }
        pos += count;
        if (count & 1)
        {
            if (pos == size)
                return fail("Missing WAV chunk padding");
            ++pos;
        }
    }
    if (!data || !data_size)
        return fail("WAV has no sample data");
    if (!channels || !((format == 1 && bits == 16) || (format == 3 && bits == 32)) ||
        align != channels * (bits / 8))
        return fail("Supported WAV formats are PCM16 and float32");
    if (data_size % align)
        return fail("Incomplete WAV audio frame");
    size_t count = data_size / align;
    unsigned frame = clm ? clm : cue ? cue : srge ? srge : smpl;
    bool explicit_size = false;
    if (!frame && wav_frame_size > 0)
    {
        frame = wav_frame_size;
        explicit_size = true;
    }
    unsigned flags = bits == 16 ? 4 : 0;
    if (explicit_size && bits == 16)
        flags |= 8;
    bool sample = !frame;
    if (sample)
    {
        flags |= 1;
        frame = srgo ? srgo : 1024;
        while (size_t(frame) * 4 > count && frame > 8)
            frame /= 2;
    }
    if (!frame || count / frame > 512)
        return fail("WAV exceeds supported frame count");
    return build(frame, unsigned(count / frame), flags, data, data_size, channels, sample, error);
}
TableHandle default_wavetable()
{
    static auto t = [] {
        std::string error;
        return load_table(default_table, default_table_size, error);
    }();
    return t;
}
TableHandle window_wavetable()
{
    static auto t = [] {
        std::string error;
        return load_table(window_table, window_table_size, error);
    }();
    return t;
}
unsigned table_frames(const TableHandle &t)
{
    return t ? t->table.n_tables : 0;
}
unsigned table_samples(const TableHandle &t)
{
    return t ? t->table.size : 0;
}
unsigned table_flags(const TableHandle &t)
{
    return t ? t->table.flags : 0;
}
const std::vector<uint8_t> &table_bytes(const TableHandle &t)
{
    static const std::vector<uint8_t> empty;
    return t ? t->bytes : empty;
}
} // namespace surge_mm
