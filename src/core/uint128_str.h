#pragma once

#include <stdexcept>
#include <string>

#include "core/core.h"

/**
 * User-defined literal for uint128_t from an unsigned long long literal.
 * Usage: auto x = 12345678901234567890_u128;
 */
constexpr uint128_t operator"" _u128(unsigned long long value) {
    return static_cast<uint128_t>(value);
}
constexpr uint128_t strtou128(const char* str, size_t len) {
    uint128_t result = 0;
    for (size_t i = 0; i < len; ++i) {
        if (str[i] < '0' || str[i] > '9')
            throw std::invalid_argument("Invalid digit in _u128 literal");
        result = result * 10 + (str[i] - '0');
        if (i + 2 < len && str[i + 1] == '\'') ++i;  // Skip the apostrophe
    }
    return result;
}
constexpr uint128_t strtou128(std::string_view str) {
    return strtou128(str.data(), str.size());
}

/**
 * User-defined literal for uint128_t from a string literal.
 * Usage: auto x = "12345678901234567890123456789012345678"_u128;
 * Supports embedded apostrophes for digit grouping.
 */
constexpr uint128_t operator"" _u128(const char* str, size_t len) {
    return strtou128(str, len);
}

/**
 * Converts a uint128_t value to its decimal string representation.
 */
inline std::string to_string(uint128_t value) {
    if (value == 0) return "0";
    std::string result;
    while (value > 0) {
        result.insert(result.begin(), '0' + static_cast<char>(value % 10));
        value /= 10;
    }
    return result;
}