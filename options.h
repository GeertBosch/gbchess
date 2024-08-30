#include <cstddef>

#pragma once

namespace options {
const char* const name = "gbchess";
const char* const version = "0.1";

constexpr int quiescenceDepth = 6;
constexpr size_t transpositionTableEntries = 1ull << 20;
};  // namespace options
