#pragma once

#include <array>
#include <cstdint>

namespace eu4dll::features::date_formatting {

inline constexpr std::array<std::uint8_t, 10> kYearMonthDayFormat{{
    0x79, 0x20, 0x0F, 0x20, 0x6D, 0x77, 0x20, 0x64, 0x20, 0x0E}};

} // namespace eu4dll::features::date_formatting
