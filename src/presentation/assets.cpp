#include "opennfh/presentation/assets.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <string>
#include <utility>

namespace opennfh::presentation {
namespace {

const content::ObjectDef* definition(const simulation::WorldState& world, std::string_view name) {
    const auto found = world.level.objects.find(std::string(name));
    return found == world.level.objects.end() ? nullptr : &found->second;
}

const content::ObjectDef* graphics(const simulation::WorldState& world, const content::ObjectDef* object) {
    for (int depth = 0; object && depth < 8; ++depth) {
        const auto* next = definition(world, object->gfx);
        if (!next || next == object) break;
        object = next;
    }
    return object;
}

Vec2i pair(std::string_view text) {
    Vec2i result;
    const auto slash = text.find('/');
    if (slash == std::string_view::npos) return result;
    std::from_chars(text.data(), text.data() + slash, result.x);
    std::from_chars(text.data() + slash + 1, text.data() + text.size(), result.y);
    return result;
}

int coordinate(std::int64_t value) {
    return static_cast<int>(std::clamp(value, std::int64_t{std::numeric_limits<int>::min()},
                                      std::int64_t{std::numeric_limits<int>::max()}));
}

Vec2i origin(const simulation::WorldState& world, const simulation::EntityState& entity) {
    auto result = entity_world_position(world, entity);
    const auto* object = definition(world, entity.kind);
    if (object && object->kind == "actor") {
        const auto anchor = pair(object->hotspot);
        result.x = coordinate(std::int64_t{result.x} - anchor.x);
        result.y = coordinate(std::int64_t{result.y} - anchor.y);
    }
    return result;
}

bool same_name(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

const content::AnimationDef* animation(const content::ObjectDef* instance,
                                      const content::ObjectDef* group,
                                      std::string_view requested) {
    const auto lookup = [&](std::string_view name) -> const content::AnimationDef* {
        for (const auto* object : {instance, group}) {
            if (!object) continue;
            const auto found = object->animations.find(std::string(name));
            if (found != object->animations.end()) return &found->second;
        }
        return nullptr;
    };
    if (!requested.empty()) {
        if (const auto* found = lookup(requested)) return found;
    }
    for (const auto name : {"ms", "ms2", "ms0", "idle", "ms1", "ms3"}) {
        if (const auto* found = lookup(name)) return found;
    }
    return nullptr;
}

struct Sprite {
    std::string path;
    Vec2i offset;
    const content::AnimationDef* animation{nullptr};
};

Sprite sprite(const simulation::WorldState& world, const simulation::EntityState& entity,
              simulation::Tick tick) {
    Sprite result;
    const auto* instance = definition(world, entity.kind);
    const auto* group = graphics(world, instance);
    if (!group || entity.animation == "inv") return result;
    result.animation = animation(instance, group, entity.animation);
    std::string file;
    if (result.animation) {
        // Empty idle timelines are intentional: this object is painted into the background.
        if (result.animation->frames.empty()) return result;
        const auto elapsed = tick >= entity.animation_started ? tick - entity.animation_started : 0;
        const auto count = result.animation->frames.size();
        const auto index = result.animation->type == "loop" ? elapsed % count : std::min<simulation::Tick>(elapsed, count - 1);
        file = result.animation->frames[static_cast<std::size_t>(index)].gfx;
        if (file.empty()) return result;
    } else if (!group->gfx_files.empty()) {
        file = group->gfx_files.front().image;
    } else if (instance->gfx.ends_with(".tga") || instance->gfx.ends_with(".png")) {
        result.path = instance->gfx;
        return result;
    } else {
        return result;
    }
    for (const auto& value : group->gfx_files) {
        if (same_name(value.image, file)) { result.offset = value.offset; break; }
    }
    const auto prefix = !instance->gfx.empty() ? instance->gfx : group->name;
    result.path = file.find('/') == std::string::npos && !prefix.empty() ? prefix + "/" + file : file;
    return result;
}

} // namespace

Vec2i entity_world_position(const simulation::WorldState& world, const simulation::EntityState& entity) {
    auto result = entity.position;
    for (const auto& room : world.level.rooms) {
        if (room.id == entity.room) {
            result.x = coordinate(std::int64_t{result.x} + room.offset.x);
            result.y = coordinate(std::int64_t{result.y} + room.offset.y);
            break;
        }
    }
    return result;
}

Result<io::ImageRgba8> load_entity_image(const io::DataRoot& root,
                                      const simulation::WorldState& world,
                                      simulation::EntityId id, simulation::Tick tick) {
    for (const auto& entity : world.entities) {
        if (entity.id != id) continue;
        const auto ref = sprite(world, entity, tick);
        if (ref.path.empty()) return Result<io::ImageRgba8>::failure(
            {ErrorCode::Missing, "entity has no sprite in its current animation", entity.kind});
        auto bytes = root.gfx_data().read(ref.path);
        // Legacy manifests may already store an archive-relative frame name.
        if (!bytes.has_value()) {
            const auto slash = ref.path.find_last_of('/');
            if (slash != std::string::npos) bytes = root.gfx_data().read(ref.path.substr(slash + 1));
        }
        if (!bytes.has_value()) return Result<io::ImageRgba8>::failure(bytes.error());
        auto image = ref.path.ends_with(".png") ? io::decode_png(bytes.value()) : io::decode_tga(bytes.value());
        if (!image.has_value()) {
            auto error = image.error();
            error.source = ref.path;
            return Result<io::ImageRgba8>::failure(std::move(error));
        }
        return image;
    }
    return Result<io::ImageRgba8>::failure({ErrorCode::Missing, "render entity is missing"});
}

RenderSnapshot make_render_snapshot(const simulation::WorldState& world, simulation::Tick tick) {
    RenderSnapshot snapshot;
    snapshot.logical_size = world.level.meta.size;
    for (std::size_t index = 0; index < world.entities.size(); ++index) {
        const auto& entity = world.entities[index];
        if (!entity.active || !entity.visible) continue;
        const auto ref = sprite(world, entity, tick);
        if (ref.path.empty()) continue;
        auto position = origin(world, entity);
        position.x = coordinate(std::int64_t{position.x} + ref.offset.x);
        position.y = coordinate(std::int64_t{position.y} + ref.offset.y);
        snapshot.items.push_back({entity.id, ref.path, position, entity.layer,
                                  entity_world_position(world, entity).y, index});
    }
    return snapshot;
}

std::vector<simulation::HitRegion> make_hit_regions(
    const simulation::WorldState& world, const RenderSnapshot& snapshot,
    const AssetCache& assets, simulation::Tick tick) {
    std::vector<simulation::HitRegion> result;
    for (std::size_t index = 0; index < world.entities.size(); ++index) {
        const auto& entity = world.entities[index];
        if (!entity.active || !entity.visible) continue;
        const auto* instance = definition(world, entity.kind);
        if (!instance || instance->kind == "actor" ||
            (instance->actions.empty() && instance->standard_actions.empty() &&
                          instance->kind != "door")) continue;
        const auto* group = graphics(world, instance);
        if (!group) continue;
        const auto ref = sprite(world, entity, tick);
        const auto base = origin(world, entity);
        const auto before = result.size();
        const auto add = [&](const std::vector<content::RegionDef>& regions) {
            for (const auto& region : regions) {
                if (region.type == "text" || region.size.x <= 0 || region.size.y <= 0) continue;
                result.push_back({entity.id,
                    {coordinate(std::int64_t{base.x} + region.position.x),
                     coordinate(std::int64_t{base.y} + region.position.y)},
                    region.size, entity.layer, entity_world_position(world, entity).y, index, true});
            }
        };
        add(group->regions);
        if (instance != group) add(instance->regions);
        if (ref.animation) add(ref.animation->regions);
        if (result.size() != before) continue;
        for (const auto& item : snapshot.items) {
            if (item.entity != entity.id) continue;
            if (const auto* image = assets.find(item.asset_id)) {
                result.push_back({entity.id, item.position,
                    {image->info.width, image->info.height}, item.layer, item.y_order, item.source_order, true});
            }
        }
    }
    return result;
}

} // namespace opennfh::presentation
