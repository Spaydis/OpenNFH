#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "opennfh/io/text_codec.hpp"

namespace {

std::vector<std::byte> utf16le_ascii(const char* text) {
    std::vector<std::byte> bytes = {
        static_cast<std::byte>(0xff), static_cast<std::byte>(0xfe),
    };
    for (const auto* cursor = text; *cursor != '\0'; ++cursor) {
        bytes.push_back(static_cast<std::byte>(*cursor));
        bytes.push_back(static_cast<std::byte>(0));
    }
    return bytes;
}

}  // namespace

int main() {
    const auto utf16 = opennfh::io::decode_xml_bytes(utf16le_ascii("<dialog>ok</dialog>"));
    assert(utf16.has_value());
    assert(utf16.value() == "<dialog>ok</dialog>");

    const std::vector<std::byte> utf8_bom = {
        static_cast<std::byte>(0xef), static_cast<std::byte>(0xbb),
        static_cast<std::byte>(0xbf), static_cast<std::byte>('<'),
        static_cast<std::byte>('x'), static_cast<std::byte>('/'),
        static_cast<std::byte>('>'),
    };
    const auto utf8 = opennfh::io::decode_xml_bytes(utf8_bom);
    assert(utf8.has_value());
    assert(utf8.value() == "<x/>");

    const std::vector<std::byte> cp1252 = {
        static_cast<std::byte>('<'), static_cast<std::byte>('x'),
        static_cast<std::byte>('>'), static_cast<std::byte>(0xe9),
        static_cast<std::byte>('<'), static_cast<std::byte>('/'),
        static_cast<std::byte>('x'), static_cast<std::byte>('>'),
    };
    const auto legacy = opennfh::io::decode_xml_bytes(cp1252);
    assert(legacy.has_value());
    const std::string expected = "<x>\xC3\xA9</x>";
    assert(legacy.value() == expected);
    return 0;
}
