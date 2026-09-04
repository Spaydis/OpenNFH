#include <iostream>
#include <string_view>

#include "opennfh/build_info.hpp"

namespace {

void print_help() {
    std::cout << "OpenNFH\n"
              << "  --data-root <path>  use a user-owned data directory\n"
              << "  --inspect           inspect local pack metadata\n"
              << "  --replay <path>     run a deterministic replay\n"
              << "  --help              show this help\n";
}

bool takes_value(std::string_view argument) {
    return argument == "--data-root" || argument == "--replay";
}

}  // namespace

int main(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help") {
            print_help();
            return 0;
        }
        if (takes_value(argument)) {
            if (index + 1 >= argc) {
                std::cerr << argument << " requires a value\n";
                return 2;
            }
            ++index;
            continue;
        }
        if (argument == "--inspect") {
            continue;
        }
        std::cerr << "unknown option: " << argument << '\n';
        return 2;
    }

    if (argc == 1) {
        print_help();
    }
    return 0;
}
