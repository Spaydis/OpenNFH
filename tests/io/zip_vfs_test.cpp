#include <cassert>
#include <cstddef>
#include <string>

#include "opennfh/io/zip_vfs.hpp"
#include "support/zip_fixture.hpp"

int main() {
    test_support::ZipFixture fixture;
    const auto result = opennfh::io::ZipVfs::open(fixture.path());
    assert(result.has_value());

    const auto& archive = result.value();
    assert(archive.contains("stored\\value.txt"));
    assert(archive.contains("nested/value.txt"));
    assert(!archive.contains("missing.txt"));

    const auto stored = archive.read("stored/value.txt");
    assert(stored.has_value());
    const std::string stored_text(reinterpret_cast<const char*>(stored.value().data()), stored.value().size());
    assert(stored_text == "stored-value");

    const auto deflated = archive.read("nested/value.txt");
    assert(deflated.has_value());
    const std::string deflated_text(reinterpret_cast<const char*>(deflated.value().data()), deflated.value().size());
    assert(deflated_text == "deflated-value");

    const auto missing = archive.read("missing.txt");
    assert(!missing.has_value());
    assert(missing.error().code == opennfh::ErrorCode::Missing);

    const auto entries = archive.entries();
    assert(entries.size() == 3);
    return 0;
}
