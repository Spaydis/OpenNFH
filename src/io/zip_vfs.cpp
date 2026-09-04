#include "opennfh/io/zip_vfs.hpp"

#include <limits>
#include <optional>

#include <zip.h>

namespace opennfh::io {

struct ZipVfs::State {
    std::shared_ptr<zip_t> archive;
    std::vector<ZipEntry> entries;
};

namespace {

Error make_error(ErrorCode code, std::string message) {
    Error error;
    error.code = code;
    error.message = std::move(message);
    return error;
}

std::optional<std::string> normalize_path(std::string_view input) {
    if (input.empty() || input.front() == '/' || input.front() == '\\' ||
        (input.size() > 1 && input[1] == ':')) {
        return std::nullopt;
    }

    std::string normalized;
    std::string component;
    for (const char character : input) {
        const bool separator = character == '/' || character == '\\';
        if (!separator) {
            component.push_back(character);
            continue;
        }
        if (component == "..") {
            return std::nullopt;
        }
        if (!component.empty() && component != ".") {
            if (!normalized.empty()) {
                normalized.push_back('/');
            }
            normalized += component;
        }
        component.clear();
    }
    if (component == "..") {
        return std::nullopt;
    }
    if (!component.empty() && component != ".") {
        if (!normalized.empty()) {
            normalized.push_back('/');
        }
        normalized += component;
    }
    return normalized.empty() ? std::nullopt : std::optional<std::string>(std::move(normalized));
}

}  // namespace

Result<ZipVfs> ZipVfs::open(const std::filesystem::path& archive_path) {
    int error_code = 0;
    zip_t* raw_archive = zip_open(archive_path.string().c_str(), ZIP_RDONLY, &error_code);
    if (raw_archive == nullptr) {
        return Result<ZipVfs>::failure(
            make_error(ErrorCode::Io, "cannot open ZIP archive: " + archive_path.string()));
    }

    auto state = std::make_shared<State>();
    state->archive = std::shared_ptr<zip_t>(raw_archive, [](zip_t* archive) {
        if (archive != nullptr) {
            zip_discard(archive);
        }
    });

    const zip_int64_t count = zip_get_num_entries(raw_archive, 0);
    if (count < 0) {
        return Result<ZipVfs>::failure(make_error(ErrorCode::Format, "cannot enumerate ZIP archive"));
    }
    state->entries.reserve(static_cast<std::size_t>(count));
    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(count); ++index) {
        zip_stat_t stat{};
        if (zip_stat_index(raw_archive, index, ZIP_FL_ENC_RAW, &stat) != 0 || stat.name == nullptr) {
            return Result<ZipVfs>::failure(make_error(ErrorCode::Format, "invalid ZIP entry metadata"));
        }
        state->entries.push_back(ZipEntry{
            stat.name,
            stat.size,
            stat.comp_size,
        });
    }
    return Result<ZipVfs>::success(ZipVfs(std::move(state)));
}

Result<std::vector<std::byte>> ZipVfs::read(std::string_view path) const {
    const auto normalized = normalize_path(path);
    if (!normalized.has_value()) {
        return Result<std::vector<std::byte>>::failure(
            make_error(ErrorCode::InvalidArgument, "invalid ZIP path: " + std::string(path)));
    }
    if (!state_ || !state_->archive) {
        return Result<std::vector<std::byte>>::failure(make_error(ErrorCode::Io, "ZIP archive is closed"));
    }

    zip_stat_t stat{};
    if (zip_stat(state_->archive.get(), normalized->c_str(), ZIP_FL_ENC_UTF_8, &stat) != 0) {
        return Result<std::vector<std::byte>>::failure(
            make_error(ErrorCode::Missing, "ZIP entry not found: " + *normalized));
    }
    if (stat.size > std::numeric_limits<std::size_t>::max()) {
        return Result<std::vector<std::byte>>::failure(make_error(ErrorCode::Unsupported, "ZIP entry is too large"));
    }

    zip_file_t* file = zip_fopen(state_->archive.get(), normalized->c_str(), ZIP_FL_ENC_UTF_8);
    if (file == nullptr) {
        return Result<std::vector<std::byte>>::failure(make_error(ErrorCode::Io, "cannot open ZIP entry: " + *normalized));
    }
    std::vector<std::byte> data(static_cast<std::size_t>(stat.size));
    const zip_int64_t bytes_read = zip_fread(file, data.data(), data.size());
    zip_fclose(file);
    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != stat.size) {
        return Result<std::vector<std::byte>>::failure(make_error(ErrorCode::Io, "cannot read ZIP entry: " + *normalized));
    }
    return Result<std::vector<std::byte>>::success(std::move(data));
}

bool ZipVfs::contains(std::string_view path) const noexcept {
    const auto normalized = normalize_path(path);
    return state_ && state_->archive && normalized.has_value() &&
           zip_name_locate(state_->archive.get(), normalized->c_str(), ZIP_FL_ENC_UTF_8) >= 0;
}

std::vector<ZipEntry> ZipVfs::entries() const {
    return state_ ? state_->entries : std::vector<ZipEntry>{};
}

}  // namespace opennfh::io
