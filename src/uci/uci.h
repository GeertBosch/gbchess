#pragma once

#include <iostream>

#include "core/core.h"

inline std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << to_string(mv);
}

void enterUCI(std::istream& in, std::ostream& out, std::ostream& log);
