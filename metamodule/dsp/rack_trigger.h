// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace surge_mm
{
struct RackTrigger
{
    unsigned state{2};
    bool process(float value, float low = 0, float high = 1)
    {
        if (state == 0 && value >= high)
        {
            state = 1;
            return true;
        }
        if (state == 1 && value <= low)
            state = 0;
        else if (state == 2)
        {
            if (value >= high)
                state = 1;
            else if (value <= low)
                state = 0;
        }
        return false;
    }
};
} // namespace surge_mm
