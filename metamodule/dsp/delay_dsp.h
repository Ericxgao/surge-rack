// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
namespace surge_mm
{
inline constexpr float delay_time_min = -6.643856189774724f, delay_time_range = 9.965784284662087f;
inline constexpr std::array<float, 10> delay_minimum{
    delay_time_min, delay_time_min, -1, 0, 0, -60, -60, 0, 0, 0};
inline constexpr std::array<float, 10> delay_range{
    delay_time_range, delay_time_range, 2, 1, 1, 130, 130, 4, 1, 1};
inline constexpr std::array<float, 12> delay_defaults{-delay_time_min / delay_time_range,
                                                      -delay_time_min / delay_time_range,
                                                      .5,
                                                      .5,
                                                      0,
                                                      0,
                                                      1,
                                                      .5,
                                                      0,
                                                      1,
                                                      1,
                                                      0};
struct Delay;
Delay *create_delay();
void destroy_delay(Delay *);
void delay_rack_mode(Delay *, bool);
void reset_delay(Delay *);
void delay_samplerate(Delay *, float);
void process_delay(Delay *, const std::array<float, 12> &, const std::array<float, 10> &cv,
                   const std::array<float, 2> &audio, float clock, bool connected,
                   std::array<float, 2> &out, bool rack_compat = false);
std::array<float, 2> delay_times(const Delay *);
} // namespace surge_mm
