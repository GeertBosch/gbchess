#include <iostream>

#include "common.h"
#include "search.h"

inline std::ostream& operator<<(std::ostream& os, Move mv) {
    return os << std::string(mv);
}

void enterUCI(std::istream& in, std::ostream& out, std::ostream& log);
