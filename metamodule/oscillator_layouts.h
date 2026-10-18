// Rack panel order expressed as stable native control IDs, including pitch.
#pragma once
#include <array>
template <surge_mm::Model Model> constexpr std::array<int, 8> oscillator_layout();
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Classic>()
{
    return {0, 1, 2, 3, 6, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Modern>()
{
    return {0, 1, 2, 3, 6, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Sine>()
{
    return {0, 2, 1, 3, 6, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::FM2>()
{
    return {0, 2, 3, 6, 7, 4, 5, 1};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::FM3>()
{
    return {0, 2, 3, 4, 5, 6, 7, 1};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::SHNoise>()
{
    return {0, 1, 2, 6, 7, 3, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Alias>()
{
    return {0, 1, 2, 3, 6, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::String>()
{
    return {0, 4, 1, 2, 3, 5, 6, 7};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Twist>()
{
    return {0, 1, 2, 3, 6, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Wavetable>()
{
    return {0, 1, 6, 2, 3, 7, 4, 5};
}
template <> constexpr std::array<int, 8> oscillator_layout<surge_mm::Model::Window>()
{
    return {0, 1, 2, 3, 6, 7, 4, 5};
}
