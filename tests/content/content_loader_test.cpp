#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>

#include "opennfh/content/loader.hpp"
#include "fixtures/minimal_campaign.hpp"

int main() {
    test_support::MinimalCampaignFixture fixture;
    const auto root_result = opennfh::io::DataRoot::open(fixture.root());
    if (!root_result.has_value()) {
        std::cerr << root_result.error().message << " source=" << root_result.error().source << "\n";
    }
    assert(root_result.has_value());
    const auto& root = root_result.value();

    const auto campaign = opennfh::content::load_campaign(root, {});
    assert(campaign.has_value());
    assert(campaign.value().sets.size() == 4);
    assert(campaign.value().sets[1].levels[0].resource_id == "level_mail");

    const auto level = opennfh::content::load_level(root, "level_mail", {});
    assert(level.has_value());
    assert(level.value().meta.resource_id == "level_mail");
    assert(level.value().meta.level_name == "level01");

    const auto object = level.value().objects.at("shared");
    assert(object.animations.contains("idle"));
    assert(object.actions.size() == 1);
    assert(object.actions[0].name == "use");
    assert(object.gfx == "shared");
    const auto woody = level.value().objects.at("woody");
    assert(woody.speeds.size() == 1);
    assert(woody.speeds[0].name == "mg0");
    assert(woody.speeds[0].speed == 3);
    assert(woody.speeds[0].start == 0);
    assert(woody.speeds[0].noise == 0);
    const auto pin = level.value().objects.at("pin");
    assert(pin.kind == "inventar");
    assert(pin.images.at("std") == "gui/inv/i_pin_norm.tga");
    assert(level.value().rooms[0].neighbors[0].costs == 7);
    if (const char* data_root_path = std::getenv("OPENNFH_DATA_ROOT"); data_root_path != nullptr) {
        const auto actual_root = opennfh::io::DataRoot::open(data_root_path);
        assert(actual_root.has_value());
        const auto actual_campaign = opennfh::content::load_campaign(actual_root.value(), {});
        assert(actual_campaign.has_value());
        assert(actual_campaign.value().sets.size() == 4);

        const auto strict_level = opennfh::content::load_level(actual_root.value(), "level_mail", {});
        assert(!strict_level.has_value());
        assert(strict_level.error().code == opennfh::ErrorCode::Duplicate);

        opennfh::content::LoadOptions compatibility;
        compatibility.xml.duplicate_attributes = opennfh::io::DuplicateAttributePolicy::KeepLast;
        const auto compatible_level = opennfh::content::load_level(actual_root.value(), "level_mail", compatibility);
        assert(compatible_level.has_value());
        assert(!compatible_level.value().rooms.empty());
        assert(!compatible_level.value().objects.empty());
    }
    return 0;
}
