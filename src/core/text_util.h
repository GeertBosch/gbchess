#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

inline std::vector<std::string> split(std::string line, char delim) {
    std::vector<std::string> res;
    std::string word;
    for (auto c : line)
        // Don't split on delimiters inside quoted strings
        if (c == delim && (word.front() != '"' || word.back() == '"'))
            res.emplace_back(std::exchange(word, ""));
        else
            word.push_back(c);

    if (word.size()) res.emplace_back(std::move(word));
    return res;
}

inline size_t find(const std::vector<std::string>& vec, std::string what) {
    auto it = std::find(vec.begin(), vec.end(), what);
    if (it == vec.end()) throw std::runtime_error("Missing field \"" + what + "\"");
    return it - vec.begin();
}

/** Safe positive integer parsing without exceptions - returns 0 for invalid input */
inline int parsePositiveInt(std::string_view str) {
    if (str.empty() || str.length() > 9) return 0;  // Reject input over 9 digits to avoid overflow

    int value = 0;
    for (auto digit : str)
        if (digit < '0' || digit > '9')
            return 0;
        else
            value = value * 10 + (digit - '0');

    return value;
}

