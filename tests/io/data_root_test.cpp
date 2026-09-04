#include <cassert>
#include <filesystem>

#include "opennfh/io/data_root.hpp"
#include "support/zip_fixture.hpp"

int main() {
    test_support::ZipFixture fixture;
    const auto root = std::filesystem::temp_directory_path() / "opennfh-data-root-fixture";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(fixture.path(), root / "gamedata.bnd");
    std::filesystem::copy_file(fixture.path(), root / "gfxdata.bnd");
    std::filesystem::copy_file(fixture.path(), root / "sfxdata.bnd");

    const auto result = opennfh::io::DataRoot::open(root);
    assert(result.has_value());
    assert(result.value().game_data().contains("stored/value.txt"));
    assert(result.value().gfx_data().contains("nested/value.txt"));
    assert(result.value().sfx_data().contains("stored/value.txt"));

    std::filesystem::remove_all(root, error);
    return 0;
}
