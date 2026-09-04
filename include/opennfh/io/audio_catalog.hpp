#pragma once

#include <cstdint>
#include <span>

#include "opennfh/core/result.hpp"

namespace opennfh::io {

struct AudioSpec {
    std::uint16_t format{0};
    std::uint16_t channels{0};
    std::uint16_t bits{0};
    std::uint32_t sample_rate{0};
};

[[nodiscard]] Result<AudioSpec> inspect_audio(std::span<const std::byte> bytes);

}  // namespace opennfh::io
