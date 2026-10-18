// SPDX-License-Identifier: GPL-3.0-or-later
#include "SineOscillator.h"
namespace surge_mm::ringmod
{
float original_sine_shape(float s, float c, int mode)
{
    return SineOscillator::valueFromSinAndCos(s, c, mode);
}
} // namespace surge_mm::ringmod
