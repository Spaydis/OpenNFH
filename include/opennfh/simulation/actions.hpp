#pragma once

#include <string>

#include "opennfh/core/result.hpp"

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

struct ActionRequest {
    EntityId actor{0};
    EntityId target{0};
    std::string action_name;
};

struct ActionTransaction {
    EntityId actor{0};
    EntityId target{0};
    Tick started{0};
    Tick duration{0};
    std::string actor_animation;
    std::string object_animation;
    int noise{0};
    bool committed{false};
    std::string actor_next_animation;
    std::string object_next_animation;
};

[[nodiscard]] Result<ActionTransaction> begin_action(
    WorldState& world,
    const ActionRequest& request,
    Tick now);

void advance_action(WorldState& world, ActionTransaction& transaction, Tick now);

}  // namespace opennfh::simulation
