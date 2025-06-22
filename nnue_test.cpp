#include <iostream>
#include <string>

#include "nnue.h"

int process_argument(const std::string& arg) try {
    auto nnue = nnue::loadNNUE(arg);
    std::cout << "NNUE loaded successfully from: " << arg << std::endl;
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Failed to load NNUE: " << e.what() << std::endl;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) return process_argument("nn-82215d0fd0df.nnue");

    while (++argv, --argc)
        if (auto error = process_argument(*argv)) return error;

    return 0;
}