#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

RoomId WorldState::current_room(EntityId entity) const {
    for (const auto& state : entities) {
        if (state.id == entity && state.active) {
            return state.room;
        }
    }
    return {};
}

}  // namespace opennfh::simulation
