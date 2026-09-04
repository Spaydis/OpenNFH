#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "opennfh/content/loader.hpp"
#include "opennfh/io/data_root.hpp"
#include "opennfh/presentation/audio.hpp"
#include "opennfh/presentation/ui.hpp"

int main() {
    const char* path = std::getenv("OPENNFH_DATA_ROOT");
    if (path == nullptr || *path == '\0') {
        std::cout << "SKIPPED: OPENNFH_DATA_ROOT is not configured\n";
        return 0;
    }

    const auto root = opennfh::io::DataRoot::open(path);
    assert(root.has_value());

    const auto campaign = opennfh::content::load_campaign(root.value(), {});
    assert(campaign.has_value());
    assert(campaign.value().sets.size() == 4);

    opennfh::content::LoadOptions compatibility;
    compatibility.xml.duplicate_attributes = opennfh::io::DuplicateAttributePolicy::KeepLast;
    const auto level = opennfh::content::load_level(root.value(), "level_mail", compatibility);
    assert(level.has_value());
    assert(!level.value().rooms.empty());
    assert(!level.value().objects.empty());

    const auto dialog = opennfh::presentation::load_dialog(root.value(), "menu");
    assert(dialog.has_value());
    assert(dialog.value().controls.size() >= 2);

    const auto audio = opennfh::presentation::load_audio_catalog(root.value());
    assert(audio.has_value());
    assert(!audio.value().sounds.empty());
    return 0;
}
