#pragma once

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <zip.h>

namespace test_support {

class MinimalCampaignFixture {
public:
    MinimalCampaignFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("opennfh-content-fixture-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
        write_archive("gamedata.bnd", {
            {"leveldata.xml", R"(<?xml version="1.0"?>
<leveldata>
  <set name="tutorial" state="playable">
    <level name="tutorial_1" state="playable" reachable="1" minquota="100" time="0"/>
  </set>
  <set name="set01" state="playable" nextset="set02">
    <level name="level_mail" state="playable" reachable="6" minquota="60" time="3600"/>
  </set>
  <set name="set02" state="locked" nextset="set03"/>
  <set name="set03" state="locked"/>
</leveldata>)"},
            {"generic/objects.xml", R"(<object name="shared" gfx="shared"/>)"},
            {"generic/anims.xml", R"(<all_objects><object name="shared"><animation name="idle" type="loop"><frame gfx="idle.tga"/></animation></object></all_objects>)"},
            {"generic/gfxdata.xml", R"(<object name="shared"><gfxdata><file image="shared.tga" offset="0/0"/></gfxdata></object>)"},
            {"generic/sfxdata.xml", R"(<sfxdata/>)"},
            {"generic/strings.xml", R"(<strings/>)"},
            {"generic/trigger.xml", R"(<triggers/>)"},
            {"level_mail/level.xml", R"(<level name="level01" size="10/10" angrytime="1"><room name="room" offset="0/0" path1="0/0" path2="10/0"/></level>)"},
            {"level_mail/objects.xml", R"(<object name="shared"><action name="use" actor="woody" actoranim="use" time="1" noise="0"/></object>)"},
            {"level_mail/anims.xml", R"(<all_objects/>)"},
            {"level_mail/gfxdata.xml", R"(<object name="shared"><gfxdata/></object>)"},
            {"level_mail/sfxdata.xml", R"(<sfxdata/>)"},
            {"level_mail/strings.xml", R"(<strings/>)"},
            {"level_mail/tricks.xml", R"(<tricks/>)"},
            {"level_mail/combine.xml", R"(<combine/>)"},
            {"level_mail/trigger.xml", R"(<triggers/>)"},
        });
        write_archive("gfxdata.bnd", {{"empty", ""}});
        write_archive("sfxdata.bnd", {{"empty", ""}});
    }

    ~MinimalCampaignFixture() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    MinimalCampaignFixture(const MinimalCampaignFixture&) = delete;
    MinimalCampaignFixture& operator=(const MinimalCampaignFixture&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

private:
    using Entry = std::pair<std::string, std::string>;

    void write_archive(const char* name, std::initializer_list<Entry> source_entries) {
        const auto path = root_ / name;
        int error = 0;
        zip_t* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        assert(archive != nullptr);
        std::vector<Entry> entries(source_entries);
        for (auto& [entry_name, contents] : entries) {
            zip_source_t* source = zip_source_buffer(archive, contents.data(), contents.size(), 0);
            assert(source != nullptr);
            assert(zip_file_add(archive, entry_name.c_str(), source, ZIP_FL_ENC_UTF_8) >= 0);
        }
        assert(zip_close(archive) == 0);
    }

    std::filesystem::path root_;
};

}  // namespace test_support
