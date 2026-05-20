#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using StringVector = std::vector<std::string>;

inline StringVector split(std::string line, char delim) {
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

inline std::string join(const StringVector& parts, char delim) {
    std::string res;
    for (auto&& part : parts) res += res.empty() ? part : delim + part;
    return res;
}

inline size_t find(const StringVector& vec, std::string what) {
    auto it = std::find(vec.begin(), vec.end(), what);
    if (it == vec.end()) throw std::runtime_error("Missing field \"" + what + "\"");
    return it - vec.begin();
}

inline bool startsWith(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
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

/** Safe integer parsing without exceptions - returns 0 for invalid input */
inline int parseInt(std::string_view str) {
    if (startsWith(str, "-")) return -parsePositiveInt(str.substr(1));
    return parsePositiveInt(str);
}
