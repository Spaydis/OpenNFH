#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <zip.h>

#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/assets.hpp"

namespace {

void put_u16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xff);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8);
}

std::vector<std::byte> one_pixel_tga() {
    std::vector<std::byte> tga(21);
    tga[2] = static_cast<std::byte>(2);
    put_u16(tga, 12, 1);
    put_u16(tga, 14, 1);
    tga[16] = static_cast<std::byte>(24);
    tga[17] = static_cast<std::byte>(0x20);
    tga[18] = static_cast<std::byte>(3);
    tga[19] = static_cast<std::byte>(2);
    tga[20] = static_cast<std::byte>(1);
    return tga;
}

void add(zip_t* archive, const char* path, const std::vector<std::byte>& bytes) {
    zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
    assert(source != nullptr);
    assert(zip_file_add(archive, path, source, ZIP_FL_ENC_UTF_8) >= 0);
}

void create_archive(
    const std::filesystem::path& path,
    const char* entry_path,
    const std::vector<std::byte>& bytes) {
    int error = 0;
    zip_t* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    assert(archive != nullptr);
    add(archive, entry_path, bytes);
    assert(zip_close(archive) == 0);
}

class AssetFixture {
public:
    AssetFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("opennfh-assets-fixture-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
        const std::vector<std::byte> placeholder = {static_cast<std::byte>(0)};
        create_archive(root_ / "gamedata.bnd", "placeholder", placeholder);
        create_archive(root_ / "sfxdata.bnd", "placeholder", placeholder);
        create_archive(root_ / "gfxdata.bnd", "device/pixel.tga", one_pixel_tga());
    }

    ~AssetFixture() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

private:
    std::filesystem::path root_;
};

opennfh::simulation::WorldState make_world() {
    using namespace opennfh;
    using namespace opennfh::content;
    using namespace opennfh::simulation;

    WorldState world;
    world.level.meta.size = {948, 868};
    world.entities = {
        EntityState{1, "device", "room", {30, 40}, 2, true},
        EntityState{2, "hidden", "room", {0, 0}, 1, false},
    };
    ObjectDef device;
    device.name = "device";
    device.gfx = "device";
    device.gfx_files.push_back(GfxFile{"pixel.tga", {3, 4}});
    world.level.objects.emplace("device", std::move(device));
    return world;
}

}  // namespace

int main() {
    auto world = make_world();
    const auto snapshot = opennfh::presentation::make_render_snapshot(world);
    assert(snapshot.logical_size.x == 948);
    assert(snapshot.items.size() == 1);
    assert(snapshot.items[0].entity == 1);
    assert(snapshot.items[0].asset_id == "pixel.tga");
    assert(snapshot.items[0].position.x == 33);
    assert(snapshot.items[0].position.y == 44);

    AssetFixture fixture;
    const auto root = opennfh::io::DataRoot::open(fixture.root());
    assert(root.has_value());
    const auto image = opennfh::presentation::load_entity_image(root.value(), world, 1);
    assert(image.has_value());
    assert(image.value().info.width == 1);
    assert(image.value().rgba[0] == 1);
    assert(image.value().rgba[1] == 2);
    assert(image.value().rgba[2] == 3);
    assert(image.value().rgba[3] == 255);

    const auto missing = opennfh::presentation::load_entity_image(root.value(), world, 99);
    assert(!missing.has_value());
    return 0;
}
