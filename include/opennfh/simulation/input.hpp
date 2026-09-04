#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "opennfh/core/result.hpp"
#include "opennfh/simulation/actions.hpp"

namespace opennfh::simulation {

struct HitRegion {
    EntityId entity{0};
    Vec2i offset;
    Vec2i size;
    int layer{0};
    int y_order{0};
    std::uint64_t source_order{0};
    bool active{true};
};

[[nodiscard]] Result<EntityId> hit_test(
    std::span<const HitRegion> regions,
    Vec2i cursor);

[[nodiscard]] Result<EntityId> resolve_target(
    const WorldState& world,
    std::string_view target);

[[nodiscard]] Result<ActionRequest> action_request_for(
    const WorldState& world,
    EntityId actor,
    EntityId target,
    std::string_view explicit_action = {});

}  // namespace opennfh::simulation
