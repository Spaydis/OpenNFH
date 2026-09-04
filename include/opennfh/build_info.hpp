#pragma once

#include <string_view>

namespace opennfh::build {

[[nodiscard]] constexpr std::string_view name() noexcept { return "OpenNFH"; }

}  // namespace opennfh::build
