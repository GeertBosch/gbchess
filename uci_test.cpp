#include "uci.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

void fromStdIn() {
    std::ofstream log("uci_test.log");
    log << "Entering UCI\n";
    enterUCI(std::cin, std::cout, log);
}

void fromFile(const char* filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        enterUCI(file, std::cout, std::cout);
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::exit(2);
    }
}

int main(int argc, char** argv) {
    if (argc == 1)
        fromStdIn();
    else
        for (int i = 1; i < argc; i++) fromFile(argv[i]);

    return 0;
}