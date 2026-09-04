#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "opennfh/core/result.hpp"

namespace opennfh::io {

struct ZipEntry {
    std::string path;
    std::uint64_t size{0};
    std::uint64_t compressed_size{0};
};

class ZipVfs {
public:
    static Result<ZipVfs> open(const std::filesystem::path& archive_path);

    [[nodiscard]] Result<std::vector<std::byte>> read(std::string_view path) const;
    [[nodiscard]] bool contains(std::string_view path) const noexcept;
    [[nodiscard]] std::vector<ZipEntry> entries() const;

private:
    struct State;

    explicit ZipVfs(std::shared_ptr<State> state) : state_(std::move(state)) {}

    std::shared_ptr<State> state_;
};

}  // namespace opennfh::io
