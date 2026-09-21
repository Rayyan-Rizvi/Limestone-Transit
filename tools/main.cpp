#include <iostream>
#include <string>

#include "limestone/version.hpp"

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "limestone " << limestone::version() << '\n';
        return 0;
    }

    std::cerr << "usage: limestone plan FROM TO --at HH:MM\n";
    return 2;
}