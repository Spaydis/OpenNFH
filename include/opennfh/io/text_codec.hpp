#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "opennfh/core/result.hpp"

namespace opennfh::io {

[[nodiscard]] Result<std::string> decode_xml_bytes(std::span<const std::byte> bytes);

}  // namespace opennfh::io
