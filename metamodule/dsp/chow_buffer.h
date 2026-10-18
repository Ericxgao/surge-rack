// SPDX-License-Identifier: GPL-3.0-or-later
// Only the storage operations used by the pinned ChowDSP delay line.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace surge_mm::chow_host
{
template <class T> struct HeapBlock
{
    std::vector<T> data;
};
template <class T> T jmax(T a, T b)
{
    return std::max(a, b);
}
template <class T> T jlimit(T lo, T hi, T x)
{
    return std::clamp(x, lo, hi);
}
namespace dsp
{
struct ProcessSpec
{
    double sampleRate;
    uint32_t maximumBlockSize, numChannels;
};
template <class T> class AudioBlock
{
    T *data{};
    size_t channels{}, samples{};

  public:
    AudioBlock() = default;
    AudioBlock(HeapBlock<char> &heap, size_t ch, size_t n) : channels(ch), samples(n)
    {
        heap.data.resize(ch * n * sizeof(T));
        data = reinterpret_cast<T *>(heap.data.data());
    }
    size_t getNumChannels() const
    {
        return channels;
    }
    size_t getNumSamples() const
    {
        return samples;
    }
    T *getChannelPointer(size_t ch) const
    {
        return data + ch * samples;
    }
    void clear()
    {
        if (data)
            std::fill_n(data, channels * samples, T{});
    }
    void copyFrom(const AudioBlock &other)
    {
        std::copy_n(other.data, channels * samples, data);
    }
};
} // namespace dsp
} // namespace surge_mm::chow_host
