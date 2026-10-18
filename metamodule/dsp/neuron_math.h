// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <type_traits>
namespace chowdsp
{
template <class T> void mul_block(const T *in, T gain, T *out, int quads)
{
    for (int i = 0; i < quads * 4; ++i)
        out[i] = in[i] * gain;
}
} // namespace chowdsp
