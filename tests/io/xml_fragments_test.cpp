#include <cassert>
#include <string>

#include "opennfh/io/xml_fragments.hpp"

int main() {
    const auto ordered = opennfh::io::parse_xml_fragments(
        "ordered.xml", "<object name=\"a\"/><object name=\"b\"/>", {});
    assert(ordered.has_value());
    assert(ordered.value().roots.size() == 2);
    assert(ordered.value().roots[0].name == "object");
    assert(ordered.value().roots[1].attributes[0].second == "b");

    const std::string duplicate = "<object name=\"a\" actor=\"first\" actor=\"last\"/>";
    const auto strict = opennfh::io::parse_xml_fragments(
        "duplicate.xml", duplicate, {opennfh::io::DuplicateAttributePolicy::Error});
    assert(!strict.has_value());
    assert(strict.error().code == opennfh::ErrorCode::Duplicate);
    assert(strict.error().line == 1);
    assert(strict.error().column > 0);

    const auto keep_first = opennfh::io::parse_xml_fragments(
        "duplicate.xml", duplicate, {opennfh::io::DuplicateAttributePolicy::KeepFirst});
    assert(keep_first.has_value());
    assert(keep_first.value().roots[0].attributes[1].second == "first");

    const auto keep_last = opennfh::io::parse_xml_fragments(
        "duplicate.xml", duplicate, {opennfh::io::DuplicateAttributePolicy::KeepLast});
    assert(keep_last.has_value());
    assert(keep_last.value().roots[0].attributes[1].second == "last");
    assert(!keep_last.value().diagnostics.empty());

    const auto empty = opennfh::io::parse_xml_fragments(
        "empty.xml", "<?xml version=\"1.0\"?>\n  \n", {});
    assert(empty.has_value());
    assert(empty.value().roots.empty());
    return 0;
}
