#pragma once

#include <string>
#include <string_view>

#include <SDL3/SDL.h>

#include "opennfh/content/model.hpp"
#include "opennfh/core/result.hpp"
#include "opennfh/io/data_root.hpp"

namespace opennfh::presentation {

struct LiveOptions {
    int window_width{1280};
    int window_height{720};
    bool integer_scale{false};
    std::string dialog_id{"menu"};
    int logic_fps{12};
};

[[nodiscard]] Result<LiveOptions> validate_live_options(LiveOptions options);

[[nodiscard]] Result<bool> validate_play_request(
    bool headless,
    std::string_view data_root,
    std::string_view level);

[[nodiscard]] Result<int> run_level(
    const io::DataRoot& root,
    content::LevelDefinition level,
    LiveOptions options = {});

}  // namespace opennfh::presentation
