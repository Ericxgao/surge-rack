// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "table_data.h"
#include "Wavetable.h"
namespace surge_mm
{
struct TableData
{
    Wavetable table;
    std::vector<uint8_t> bytes;
};
} // namespace surge_mm
