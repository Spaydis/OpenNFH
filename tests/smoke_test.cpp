#include <cassert>

#include "opennfh/build_info.hpp"

int main() {
    assert(opennfh::build::name() == "OpenNFH");
    return 0;
}
