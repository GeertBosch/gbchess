#pragma once

#include <algorithm>
#include <string>
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
