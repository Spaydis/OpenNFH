#pragma once

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

#include <zip.h>

namespace test_support {

class ZipFixture {
public:
    ZipFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("opennfh-zip-fixture-" + std::to_string(stamp) + ".zip");

        int error = 0;
        zip_t* archive = zip_open(path_.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        assert(archive != nullptr);

        add(archive, "stored/value.txt", stored_, ZIP_CM_STORE);
        add(archive, "nested/value.txt", deflated_, ZIP_CM_DEFLATE);
        assert(zip_dir_add(archive, "explicit/", ZIP_FL_ENC_UTF_8) >= 0);
        assert(zip_close(archive) == 0);
    }

    ~ZipFixture() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    ZipFixture(const ZipFixture&) = delete;
    ZipFixture& operator=(const ZipFixture&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    static void add(zip_t* archive, const char* name, std::string& value, zip_int32_t method) {
        zip_source_t* source = zip_source_buffer(archive, value.data(), value.size(), 0);
        assert(source != nullptr);
        const zip_int64_t index = zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8);
        assert(index >= 0);
        assert(zip_set_file_compression(archive, index, method, method == ZIP_CM_DEFLATE ? 9 : 0) == 0);
    }

    std::filesystem::path path_;
    std::string stored_{"stored-value"};
    std::string deflated_{"deflated-value"};
};

}  // namespace test_support
