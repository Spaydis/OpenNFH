#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "opennfh/core/result.hpp"

namespace opennfh::io {

enum class DuplicateAttributePolicy {
    Error,
    KeepFirst,
    KeepLast,
};

struct XmlParseOptions {
    DuplicateAttributePolicy duplicate_attributes{DuplicateAttributePolicy::Error};
};

struct Diagnostic {
    ErrorCode code{ErrorCode::Format};
    std::string message;
    std::string source;
    std::size_t line{0};
    std::size_t column{0};
};

struct XmlNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XmlNode> children;
    std::string text;
};

struct XmlFragmentDocument {
    std::vector<XmlNode> roots;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] Result<XmlFragmentDocument> parse_xml_fragments(
    std::string_view source,
    std::string_view utf8,
    const XmlParseOptions& options = {});

}  // namespace opennfh::io
