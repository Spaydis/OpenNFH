#include "opennfh/simulation/navigation.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace opennfh::simulation {

namespace {

struct Edge {
    std::size_t destination{0};
    DoorId door;
    Vec2i arrival;
    int cost{0};
};

Error error(ErrorCode code, std::string message) {
    Error result;
    result.code = code;
    result.message = std::move(message);
    return result;
}

std::optional<std::size_t> room_index(const WorldState& world, std::string_view name) {
    for (std::size_t index = 0; index < world.level.rooms.size(); ++index) {
        if (world.level.rooms[index].id == name) {
            return index;
        }
    }
    return std::nullopt;
}

std::string destination_room(std::string_view door) {
    const auto slash = door.find('/');
    return slash == std::string_view::npos ? std::string{} : std::string(door.substr(slash + 1));
}

const content::Door* find_door(const content::Room& room, std::string_view id) {
    for (const auto& door : room.doors) {
        if (door.id == id) return &door;
    }
    return nullptr;
}

Vec2i door_travel_position(const WorldState& world, const content::Room& room,
                            std::string_view door_id, std::string_view actor_kind) {
    const auto* door = find_door(room, door_id);
    if (door == nullptr) return {};
    Vec2i result = door->position;
    const auto object = world.level.objects.find(std::string(door_id));
    if (object == world.level.objects.end()) return result;
    for (const auto& hotspot : object->second.hotspots) {
        if (hotspot.name == actor_kind) {
            result.x += hotspot.offset.x;
            result.y += hotspot.offset.y;
            return result;
        }
    }
    for (const auto& hotspot : object->second.hotspots) {
        if (hotspot.name.empty()) {
            result.x += hotspot.offset.x;
            result.y += hotspot.offset.y;
            return result;
        }
    }
    return result;
}

std::vector<std::vector<Edge>> build_graph(const WorldState& world,
                                           std::string_view actor_kind) {
    std::vector<std::vector<Edge>> graph(world.level.rooms.size());
    for (std::size_t index = 0; index < world.level.rooms.size(); ++index) {
        const auto& room = world.level.rooms[index];
        for (const auto& neighbor : room.neighbors) {
            // NeighborLink::name is authoritative. Door IDs are resource
            // names and may not encode the actual destination room.
            auto destination = room_index(world, neighbor.name);
            if (!destination.has_value() && !neighbor.door_in.empty()) {
                destination = room_index(world, destination_room(neighbor.door_in));
            }
            if (!destination.has_value()) {
                continue;
            }
            const auto* source_door = find_door(room, neighbor.door_in);
            if (source_door == nullptr || world.blocked_doors.contains(source_door->id)) {
                continue;
            }
            const auto& destination_room_value = world.level.rooms[*destination];
            const auto* destination_door = find_door(destination_room_value, neighbor.door_out);
            if (!neighbor.door_out.empty() && destination_door != nullptr &&
                world.blocked_doors.contains(destination_door->id)) {
                continue;
            }
            graph[index].push_back(Edge{
                *destination,
                source_door->id,
                destination_door == nullptr
                    ? Vec2i{}
                    : door_travel_position(world, destination_room_value,
                                           destination_door->id, actor_kind),
                std::max(neighbor.costs, 0),
            });
        }
    }
    return graph;
}

}  // namespace

Result<std::vector<NavStep>> find_path(
    const WorldState& world,
    EntityId actor,
    std::string_view target_room,
    Vec2i target) {
    const auto start_name = world.current_room(actor);
    if (start_name.empty()) {
        return Result<std::vector<NavStep>>::failure(error(ErrorCode::Missing, "actor is not present in the world"));
    }
    const auto start = room_index(world, start_name);
    const auto destination = room_index(world, target_room);
    if (!start.has_value() || !destination.has_value()) {
        return Result<std::vector<NavStep>>::failure(error(ErrorCode::Missing, "navigation room is not present"));
    }
    if (*start == *destination) {
        return Result<std::vector<NavStep>>::success({NavStep{std::string(target_room), {}, target, 0, target}});
    }

    std::string actor_kind;
    for (const auto& entity : world.entities) {
        if (entity.id == actor && entity.active) {
            actor_kind = entity.kind;
            break;
        }
    }
    const auto graph = build_graph(world, actor_kind);
    const auto infinity = std::numeric_limits<int>::max();
    std::vector<int> distance(graph.size(), infinity);
    std::vector<std::size_t> previous_room(graph.size(), graph.size());
    std::vector<std::size_t> previous_edge(graph.size(), graph.size());
    using QueueItem = std::pair<int, std::size_t>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> queue;
    distance[*start] = 0;
    queue.push({0, *start});

    while (!queue.empty()) {
        const auto [current_distance, current] = queue.top();
        queue.pop();
        if (current_distance != distance[current]) {
            continue;
        }
        for (std::size_t edge_index = 0; edge_index < graph[current].size(); ++edge_index) {
            const auto& edge = graph[current][edge_index];
            if (current_distance > infinity - edge.cost) {
                continue;
            }
            const int candidate = current_distance + edge.cost;
            if (candidate < distance[edge.destination]) {
                distance[edge.destination] = candidate;
                previous_room[edge.destination] = current;
                previous_edge[edge.destination] = edge_index;
                queue.push({candidate, edge.destination});
            }
        }
    }

    if (distance[*destination] == infinity) {
        return Result<std::vector<NavStep>>::failure(error(ErrorCode::Missing, "no navigation route exists"));
    }

    std::vector<NavStep> path;
    for (std::size_t current = *destination; current != *start; current = previous_room[current]) {
        if (previous_room[current] == graph.size() || previous_edge[current] == graph.size()) {
            return Result<std::vector<NavStep>>::failure(error(ErrorCode::Format, "navigation predecessor chain is invalid"));
        }
        const auto& edge = graph[previous_room[current]][previous_edge[current]];
        path.push_back(NavStep{world.level.rooms[current].id, edge.door, edge.arrival, edge.cost, edge.arrival});
    }
    std::reverse(path.begin(), path.end());
    path.back().destination = target;
    return Result<std::vector<NavStep>>::success(std::move(path));
}

Result<std::vector<NavStep>> find_path(const WorldState& world, EntityId actor, Vec2i target) {
    return find_path(world, actor, world.current_room(actor), target);
}

void set_path(WorldState& world, EntityId actor, std::vector<NavStep> path) {
    world.pending_paths[actor] = std::move(path);
    world.pending_indices[actor] = 0;
}

Result<bool> walk_to(
    WorldState& world,
    EntityId actor,
    std::string_view target_room,
    Vec2i target) {
    EntityState* actor_state = nullptr;
    for (auto& entity : world.entities) {
        if (entity.id == actor && entity.active) {
            actor_state = &entity;
            break;
        }
    }
    if (actor_state == nullptr) {
        return Result<bool>::failure(error(ErrorCode::Missing, "walking actor is not active"));
    }
    if (world.busy_entities.contains(actor)) {
        return Result<bool>::failure(error(ErrorCode::InvalidArgument, "walking actor is busy"));
    }
    if (!room_index(world, target_room).has_value()) {
        return Result<bool>::failure(error(ErrorCode::Missing, "walking destination room is not present"));
    }
    const auto route = find_path(world, actor, target_room, target);
    if (!route.has_value()) return Result<bool>::failure(route.error());

    std::vector<NavStep> waypoints;
    RoomId current_room = actor_state->room;
    for (const auto& step : route.value()) {
        const auto source = room_index(world, current_room);
        if (!source.has_value() || !room_index(world, step.room).has_value()) {
            return Result<bool>::failure(error(ErrorCode::Format, "walking route references an unknown room"));
        }
        if (!step.door.empty()) {
            const auto* source_door = find_door(world.level.rooms[*source], step.door);
            if (source_door == nullptr) {
                return Result<bool>::failure(error(ErrorCode::Format, "walking route source door is missing"));
            }
            const auto departure = door_travel_position(
                world, world.level.rooms[*source], step.door, actor_state->kind);
            waypoints.push_back(NavStep{
                current_room, step.door, departure, step.cost, departure});
            waypoints.push_back(NavStep{
                step.room, step.door, step.arrival, step.cost, step.arrival});
        }
        current_room = step.room;
    }
    if (waypoints.empty() || waypoints.back().room != target_room ||
        waypoints.back().destination.x != target.x ||
        waypoints.back().destination.y != target.y) {
        waypoints.push_back(NavStep{std::string(target_room), {}, target, 0, target});
    }
    set_path(world, actor, std::move(waypoints));
    return Result<bool>::success(true);
}

void advance_walking(WorldState& world, EntityId actor, int units_per_tick) {
    if (world.busy_entities.contains(actor)) return;
    auto path = world.pending_paths.find(actor);
    auto index = world.pending_indices.find(actor);
    if (path == world.pending_paths.end() || index == world.pending_indices.end()) return;

    EntityState* state = nullptr;
    for (auto& entity : world.entities) {
        if (entity.id == actor && entity.active) {
            state = &entity;
            break;
        }
    }
    if (state == nullptr) return;

    if (units_per_tick <= 0) {
        units_per_tick = 6;
        const auto object = world.level.objects.find(state->kind);
        if (object != world.level.objects.end()) {
            for (const auto& speed : object->second.speeds) {
                if (speed.name.starts_with("mg") && speed.speed > 0) {
                    units_per_tick = speed.speed;
                    break;
                }
            }
        }
    }

    int budget = units_per_tick;
    while (budget > 0 && index->second < path->second.size()) {
        const auto& waypoint = path->second[index->second];
        if (state->room != waypoint.room) {
            // Crossing the linked door is the intentional room transition.
            state->room = waypoint.room;
            state->position = waypoint.destination;
            ++index->second;
            continue;
        }
        const int dx = waypoint.destination.x - state->position.x;
        const int dy = waypoint.destination.y - state->position.y;
        if (dx == 0 && dy == 0) {
            ++index->second;
            continue;
        }
        const int step_x = std::min(std::abs(dx), budget) * (dx < 0 ? -1 : 1);
        state->position.x += step_x;
        budget -= std::abs(step_x);
        if (budget > 0 && state->position.x == waypoint.destination.x) {
            const int step_y = std::min(std::abs(dy), budget) * (dy < 0 ? -1 : 1);
            state->position.y += step_y;
            budget -= std::abs(step_y);
        }
        if (state->position.x == waypoint.destination.x &&
            state->position.y == waypoint.destination.y) {
            ++index->second;
        }
    }
    if (index->second >= path->second.size()) {
        world.pending_paths.erase(path);
        world.pending_indices.erase(index);
    }
}

void advance_navigation(WorldState& world, EntityId actor, Tick tick) {
    (void)tick;
    const auto path = world.pending_paths.find(actor);
    const auto index = world.pending_indices.find(actor);
    if (path == world.pending_paths.end() || index == world.pending_indices.end() || index->second >= path->second.size()) {
        return;
    }
    for (auto& entity : world.entities) {
        if (entity.id == actor && entity.active) {
            const auto& step = path->second[index->second];
            entity.room = step.room;
            entity.position = step.destination;
            ++index->second;
            if (index->second == path->second.size()) {
                world.pending_paths.erase(path);
                world.pending_indices.erase(index);
            }
            return;
        }
    }
}

}  // namespace opennfh::simulation
