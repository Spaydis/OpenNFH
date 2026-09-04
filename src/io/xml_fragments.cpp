#include "opennfh/io/xml_fragments.hpp"

#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <pugixml.hpp>

namespace opennfh::io {

namespace {

struct DuplicateAttribute {
    std::string name;
    std::size_t offset{0};
};

bool is_name_character(char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_' || character == '-' ||
           character == ':' || character == '.';
}

std::pair<std::size_t, std::size_t> line_column(std::string_view text, std::size_t offset) {
    std::size_t line = 1;
    std::size_t column = 1;
    const std::size_t end = offset < text.size() ? offset : text.size();
    for (std::size_t index = 0; index < end; ++index) {
        if (text[index] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return {line, column};
}

std::vector<DuplicateAttribute> find_duplicates(std::string_view text) {
    std::vector<DuplicateAttribute> duplicates;
    for (std::size_t tag_start = 0; tag_start < text.size(); ++tag_start) {
        if (text[tag_start] != '<' || tag_start + 1 >= text.size()) {
            continue;
        }
        if (text.compare(tag_start, 4, "<!--") == 0) {
            const auto end = text.find("-->", tag_start + 4);
            tag_start = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }
        if (text.compare(tag_start, 9, "<![CDATA[") == 0) {
            const auto end = text.find("]]>", tag_start + 9);
            tag_start = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }
        if (text[tag_start + 1] == '/' || text[tag_start + 1] == '!' || text[tag_start + 1] == '?') {
            continue;
        }

        char quote = 0;
        std::size_t tag_end = tag_start + 1;
        for (; tag_end < text.size(); ++tag_end) {
            const char character = text[tag_end];
            if (quote != 0) {
                if (character == quote) {
                    quote = 0;
                }
            } else if (character == '\'' || character == '"') {
                quote = character;
            } else if (character == '>') {
                break;
            }
        }
        if (tag_end >= text.size()) {
            break;
        }

        std::size_t cursor = tag_start + 1;
        while (cursor < tag_end && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        while (cursor < tag_end && is_name_character(text[cursor])) {
            ++cursor;
        }
        std::unordered_set<std::string> seen;
        while (cursor < tag_end) {
            while (cursor < tag_end && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
                ++cursor;
            }
            if (cursor >= tag_end || text[cursor] == '/') {
                break;
            }
            const std::size_t attribute_start = cursor;
            while (cursor < tag_end && is_name_character(text[cursor])) {
                ++cursor;
            }
            if (cursor == attribute_start) {
                ++cursor;
                continue;
            }
            const std::string attribute_name(text.substr(attribute_start, cursor - attribute_start));
            if (!seen.insert(attribute_name).second) {
                duplicates.push_back({attribute_name, attribute_start});
            }
            while (cursor < tag_end && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
                ++cursor;
            }
            if (cursor < tag_end && text[cursor] == '=') {
                ++cursor;
                while (cursor < tag_end && std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
                    ++cursor;
                }
                if (cursor < tag_end && (text[cursor] == '\'' || text[cursor] == '"')) {
                    quote = text[cursor++];
                    while (cursor < tag_end && text[cursor] != quote) {
                        ++cursor;
                    }
                    if (cursor < tag_end) {
                        ++cursor;
                    }
                } else {
                    while (cursor < tag_end && !std::isspace(static_cast<unsigned char>(text[cursor])) && text[cursor] != '/') {
                        ++cursor;
                    }
                }
            }
        }
        tag_start = tag_end;
    }
    return duplicates;
}

std::string remove_xml_declaration(std::string_view text) {
    std::size_t start = 0;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        start = 3;
    }
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    if (text.compare(start, 5, "<?xml") == 0) {
        const auto end = text.find("?>", start + 5);
        if (end != std::string_view::npos) {
            start = end + 2;
        }
    }
    return std::string(text.substr(start));
}

XmlNode convert_node(const pugi::xml_node& source, DuplicateAttributePolicy policy) {
    XmlNode result;
    result.name = source.name();
    result.text = source.text().get();

    std::unordered_map<std::string, std::size_t> positions;
    for (const auto attribute : source.attributes()) {
        const std::string name = attribute.name();
        const auto [it, inserted] = positions.emplace(name, result.attributes.size());
        if (inserted) {
            result.attributes.emplace_back(name, attribute.value());
        } else if (policy == DuplicateAttributePolicy::KeepLast) {
            result.attributes[it->second].second = attribute.value();
        }
    }
    for (const auto child : source.children()) {
        if (child.type() == pugi::node_element) {
            result.children.push_back(convert_node(child, policy));
        }
    }
    return result;
}

}  // namespace

Result<XmlFragmentDocument> parse_xml_fragments(
    std::string_view source,
    std::string_view utf8,
    const XmlParseOptions& options) {
    const auto duplicates = find_duplicates(utf8);
    XmlFragmentDocument result;
    for (const auto& duplicate : duplicates) {
        const auto [line, column] = line_column(utf8, duplicate.offset);
        result.diagnostics.push_back(Diagnostic{
            ErrorCode::Duplicate,
            "duplicate XML attribute: " + duplicate.name,
            std::string(source),
            line,
            column,
        });
    }
    if (options.duplicate_attributes == DuplicateAttributePolicy::Error && !duplicates.empty()) {
        const auto& diagnostic = result.diagnostics.front();
        return Result<XmlFragmentDocument>::failure(Error{
            diagnostic.code,
            diagnostic.message,
            diagnostic.source,
            diagnostic.line,
            diagnostic.column,
        });
    }

    const std::string wrapped = "<__opennfh_fragments__>" + remove_xml_declaration(utf8) + "</__opennfh_fragments__>";
    pugi::xml_document document;
    const auto parsed = document.load_string(wrapped.c_str(), pugi::parse_default);
    if (!parsed) {
        return Result<XmlFragmentDocument>::failure(Error{
            ErrorCode::Format,
            parsed.description(),
            std::string(source),
            1,
            static_cast<std::size_t>(parsed.offset),
        });
    }

    const auto wrapper = document.child("__opennfh_fragments__");
    for (const auto child : wrapper.children()) {
        if (child.type() == pugi::node_element) {
            result.roots.push_back(convert_node(child, options.duplicate_attributes));
        }
    }
    return Result<XmlFragmentDocument>::success(std::move(result));
}

}  // namespace opennfh::io
