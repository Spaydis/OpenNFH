#include "opennfh/io/text_codec.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace opennfh::io {

namespace {

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t offset) {
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string decode_utf16(std::span<const std::byte> bytes, bool little_endian) {
    std::string output;
    output.reserve(bytes.size());
    std::size_t offset = 0;
    while (offset + 1 < bytes.size()) {
        const auto first = little_endian
                               ? static_cast<std::uint16_t>(byte_at(bytes, offset) | (byte_at(bytes, offset + 1) << 8))
                               : static_cast<std::uint16_t>((byte_at(bytes, offset) << 8) | byte_at(bytes, offset + 1));
        offset += 2;
        std::uint32_t codepoint = first;
        if (first >= 0xd800 && first <= 0xdbff && offset + 1 < bytes.size()) {
            const auto second = little_endian
                                    ? static_cast<std::uint16_t>(byte_at(bytes, offset) | (byte_at(bytes, offset + 1) << 8))
                                    : static_cast<std::uint16_t>((byte_at(bytes, offset) << 8) | byte_at(bytes, offset + 1));
            if (second >= 0xdc00 && second <= 0xdfff) {
                codepoint = 0x10000 + ((static_cast<std::uint32_t>(first) - 0xd800) << 10) + second - 0xdc00;
                offset += 2;
            } else {
                codepoint = 0xfffd;
            }
        } else if (first >= 0xdc00 && first <= 0xdfff) {
            codepoint = 0xfffd;
        }
        append_utf8(output, codepoint);
    }
    return output;
}

bool valid_utf8(std::span<const std::byte> bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto first = byte_at(bytes, offset++);
        std::uint32_t codepoint = 0;
        std::size_t continuation_count = 0;
        if (first <= 0x7f) {
            continue;
        } else if (first >= 0xc2 && first <= 0xdf) {
            codepoint = first & 0x1f;
            continuation_count = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            codepoint = first & 0x0f;
            continuation_count = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            codepoint = first & 0x07;
            continuation_count = 3;
        } else {
            return false;
        }
        if (offset + continuation_count > bytes.size()) {
            return false;
        }
        for (std::size_t index = 0; index < continuation_count; ++index) {
            const auto next = byte_at(bytes, offset++);
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if ((continuation_count == 2 && codepoint < 0x800) ||
            (continuation_count == 3 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) {
            return false;
        }
    }
    return true;
}

std::string decode_cp1252(std::span<const std::byte> bytes) {
    constexpr std::uint16_t extended[] = {
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178,
    };
    std::string output;
    output.reserve(bytes.size());
    for (const auto value : bytes) {
        const auto byte = std::to_integer<std::uint8_t>(value);
        const std::uint32_t codepoint = byte >= 0x80 && byte <= 0x9f ? extended[byte - 0x80] : byte;
        append_utf8(output, codepoint);
    }
    return output;
}

}  // namespace

Result<std::string> decode_xml_bytes(std::span<const std::byte> bytes) {
    if (bytes.size() >= 2 && byte_at(bytes, 0) == 0xff && byte_at(bytes, 1) == 0xfe) {
        return Result<std::string>::success(decode_utf16(bytes.subspan(2), true));
    }
    if (bytes.size() >= 2 && byte_at(bytes, 0) == 0xfe && byte_at(bytes, 1) == 0xff) {
        return Result<std::string>::success(decode_utf16(bytes.subspan(2), false));
    }
    if (bytes.size() >= 3 && byte_at(bytes, 0) == 0xef &&
        byte_at(bytes, 1) == 0xbb && byte_at(bytes, 2) == 0xbf) {
        bytes = bytes.subspan(3);
    }
    if (valid_utf8(bytes)) {
        return Result<std::string>::success(std::string(
            reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }
    return Result<std::string>::success(decode_cp1252(bytes));
}

}  // namespace opennfh::io
