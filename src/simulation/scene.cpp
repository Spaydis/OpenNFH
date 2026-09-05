#include "opennfh/simulation/scene.hpp"

#include <utility>

namespace opennfh::simulation {

WorldState make_world(content::LevelDefinition level) {
    WorldState world;
    world.level = std::move(level);
    EntityId next_id = 1;

    const auto add_entity = [&](EntityState state) {
        const auto id = state.id;
        const auto definition = world.level.objects.find(state.kind);
        if (definition != world.level.objects.end() &&
            !definition->second.contents.empty()) {
            auto& contents = world.object_contents[id];
            for (const auto& item : definition->second.contents) {
                if (item.count > 0) {
                    contents[item.name] = item.count;
                }
            }
        }
        world.entities.push_back(std::move(state));
    };

    for (const auto& object : world.level.root_objects) {
        add_entity(EntityState{
            next_id++, object.name, {}, object.position, object.layer, true, object.visible, object.animation});
    }
    for (const auto& room : world.level.rooms) {
        for (const auto& actor : room.actors) {
            add_entity(EntityState{
                next_id++, actor.name, room.id, actor.position, actor.layer, true, true, actor.animation});
        }
        for (const auto& object : room.objects) {
            add_entity(EntityState{
                next_id++, object.name, room.id, object.position, object.layer, true, object.visible, object.animation});
        }
        for (const auto& door : room.doors) {
            add_entity(EntityState{
                next_id++, door.id, room.id, door.position, door.layer, true, door.visible});
        }
    }
    return world;
}

}  // namespace opennfh::simulation
