#include "opennfh/io/data_root.hpp"

#include <utility>

namespace opennfh::io {

namespace {

Result<ZipVfs> open_pack(const std::filesystem::path& root, const char* name) {
    return ZipVfs::open(root / name);
}

}  // namespace

Result<DataRoot> DataRoot::open(const std::filesystem::path& root) {
    auto game_data = open_pack(root, "gamedata.bnd");
    if (!game_data.has_value()) {
        return Result<DataRoot>::failure(game_data.error());
    }

    auto gfx_data = open_pack(root, "gfxdata.bnd");
    if (!gfx_data.has_value()) {
        return Result<DataRoot>::failure(gfx_data.error());
    }

    auto sfx_data = open_pack(root, "sfxdata.bnd");
    if (!sfx_data.has_value()) {
        return Result<DataRoot>::failure(sfx_data.error());
    }

    return Result<DataRoot>::success(DataRoot(
        std::move(game_data.value()), std::move(gfx_data.value()), std::move(sfx_data.value())));
}

}  // namespace opennfh::io
