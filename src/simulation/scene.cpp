#include "opennfh/simulation/scene.hpp"

#include <utility>

namespace opennfh::simulation {

WorldState make_world(content::LevelDefinition level) {
    WorldState world;
    world.level = std::move(level);
    EntityId next_id = 1;

    for (const auto& object : world.level.root_objects) {
        world.entities.push_back(EntityState{
            next_id++, object.name, {}, object.position, object.layer, true, object.visible, object.animation});
    }
    for (const auto& room : world.level.rooms) {
        for (const auto& actor : room.actors) {
            world.entities.push_back(EntityState{
                next_id++, actor.name, room.id, actor.position, actor.layer, true, true, actor.animation});
        }
        for (const auto& object : room.objects) {
            world.entities.push_back(EntityState{
                next_id++, object.name, room.id, object.position, object.layer, true, object.visible, object.animation});
        }
        for (const auto& door : room.doors) {
            world.entities.push_back(EntityState{
                next_id++, door.id, room.id, door.position, door.layer, true, door.visible});
        }
    }
    return world;
}

}  // namespace opennfh::simulation
