#pragma once

#include <filesystem>

#include <utility>
#include "opennfh/core/result.hpp"
#include "opennfh/io/zip_vfs.hpp"

namespace opennfh::io {

class DataRoot {
public:
    static Result<DataRoot> open(const std::filesystem::path& root);

    [[nodiscard]] const ZipVfs& game_data() const noexcept { return game_data_; }
    [[nodiscard]] const ZipVfs& gfx_data() const noexcept { return gfx_data_; }
    [[nodiscard]] const ZipVfs& sfx_data() const noexcept { return sfx_data_; }

private:
    DataRoot(ZipVfs game_data, ZipVfs gfx_data, ZipVfs sfx_data)
        : game_data_(std::move(game_data)), gfx_data_(std::move(gfx_data)), sfx_data_(std::move(sfx_data)) {}

    ZipVfs game_data_;
    ZipVfs gfx_data_;
    ZipVfs sfx_data_;
};

}  // namespace opennfh::io
