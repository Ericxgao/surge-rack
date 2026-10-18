// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace surge_mm
{
struct TableData;
using TableHandle = std::shared_ptr<const TableData>;
// Loading builds Surge's mipmaps and allocates: call outside the audio callback.
// WT accepts the legacy short payloads padded by Surge's desktop loader.
TableHandle load_table(const uint8_t *bytes, size_t size, std::string &error,
                       int wav_frame_size = 0);
TableHandle default_wavetable();
TableHandle window_wavetable();
unsigned table_frames(const TableHandle &);
unsigned table_samples(const TableHandle &);
unsigned table_flags(const TableHandle &);
const std::vector<uint8_t> &table_bytes(const TableHandle &);
} // namespace surge_mm
