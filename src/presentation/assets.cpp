#include "opennfh/presentation/assets.hpp"

#include <string>
#include <utility>
#include <vector>

namespace opennfh::presentation {

namespace {

Error error(ErrorCode code, std::string message, std::string source = {}) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    result.source = std::move(source);
    return result;
}

const simulation::EntityState* find_entity(
    const simulation::WorldState& world,
    simulation::EntityId id) {
    for (const auto& entity : world.entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const content::ObjectDef* find_object(
    const simulation::WorldState& world,
    std::string_view kind) {
    const auto found = world.level.objects.find(std::string(kind));
    return found == world.level.objects.end() ? nullptr : &found->second;
}

}  // namespace

Result<io::ImageRgba8> load_entity_image(
    const io::DataRoot& root,
    const simulation::WorldState& world,
    simulation::EntityId entity_id) {
    const auto* entity = find_entity(world, entity_id);
    if (entity == nullptr) {
        return Result<io::ImageRgba8>::failure(
            error(ErrorCode::Missing, "render entity is missing"));
    }
    const auto* object = find_object(world, entity->kind);
    if (object == nullptr || object->gfx_files.empty()) {
        return Result<io::ImageRgba8>::failure(
            error(ErrorCode::Missing, "render entity has no graphics definition", entity->kind));
    }

    const auto& image = object->gfx_files.front().image;
    std::vector<std::string> candidates;
    candidates.push_back(image);
    if (!object->gfx.empty()) {
        candidates.push_back(object->gfx + "/" + image);
    }

    std::string selected;
    for (const auto& candidate : candidates) {
        if (root.gfx_data().contains(candidate)) {
            selected = candidate;
            break;
        }
    }
    if (selected.empty()) {
        return Result<io::ImageRgba8>::failure(
            error(ErrorCode::Missing, "graphics file is missing", image));
    }
    const auto bytes = root.gfx_data().read(selected);
    if (!bytes.has_value()) {
        return Result<io::ImageRgba8>::failure(bytes.error());
    }
    if (selected.size() >= 4 &&
        selected.substr(selected.size() - 4) == ".png") {
        return io::decode_png(bytes.value());
    }
    return io::decode_tga(bytes.value());
}

RenderSnapshot make_render_snapshot(const simulation::WorldState& world) {
    RenderSnapshot snapshot;
    snapshot.logical_size = world.level.meta.size;
    for (std::size_t index = 0; index < world.entities.size(); ++index) {
        const auto& entity = world.entities[index];
        if (!entity.active) {
            continue;
        }
        RenderItem item;
        item.entity = entity.id;
        item.asset_id = entity.kind;
        item.position = entity.position;
        item.layer = entity.layer;
        item.y_order = entity.position.y;
        item.source_order = index;
        if (const auto* object = find_object(world, entity.kind);
            object != nullptr && !object->gfx_files.empty()) {
            item.asset_id = object->gfx_files.front().image;
            item.position.x += object->gfx_files.front().offset.x;
            item.position.y += object->gfx_files.front().offset.y;
        }
        snapshot.items.push_back(std::move(item));
    }
    return snapshot;
}

}  // namespace opennfh::presentation
