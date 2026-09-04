#include <cassert>

#include "opennfh/presentation/live.hpp"

int main() {
    using opennfh::presentation::LiveOptions;

    const auto defaults = opennfh::presentation::validate_live_options({});
    assert(defaults.has_value());
    assert(defaults.value().window_width == 1280);
    assert(defaults.value().window_height == 720);
    assert(defaults.value().dialog_id == "menu");

    const auto bad_width = opennfh::presentation::validate_live_options(
        LiveOptions{0, 720, false, "menu"});
    assert(!bad_width.has_value());

    const auto valid_play = opennfh::presentation::validate_play_request(
        false, "data", "level_mail");
    assert(valid_play.has_value());

    const auto missing_root = opennfh::presentation::validate_play_request(
        false, {}, "level_mail");
    assert(!missing_root.has_value());

    const auto missing_level = opennfh::presentation::validate_play_request(
        false, "data", {});
    assert(!missing_level.has_value());

    const auto headless_play = opennfh::presentation::validate_play_request(
        true, "data", "level_mail");
    assert(!headless_play.has_value());
    return 0;
}
