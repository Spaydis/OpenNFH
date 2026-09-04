#include "opennfh/simulation/navigation.hpp"

#include <algorithm>
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

int edge_cost(const content::Room& room, std::string_view door) {
    for (const auto& neighbor : room.neighbors) {
        if (neighbor.door_in == door || neighbor.door_out == door) {
            return neighbor.costs;
        }
    }
    return 1;
}

Vec2i arrival_position(const content::Room& room, std::string_view from) {
    const std::string reverse = room.id + "/" + std::string(from);
    for (const auto& door : room.doors) {
        if (door.id == reverse) {
            return door.position;
        }
    }
    return {};
}

std::vector<std::vector<Edge>> build_graph(const WorldState& world) {
    std::vector<std::vector<Edge>> graph(world.level.rooms.size());
    for (std::size_t index = 0; index < world.level.rooms.size(); ++index) {
        const auto& room = world.level.rooms[index];
        for (const auto& door : room.doors) {
            if (world.blocked_doors.contains(door.id)) {
                continue;
            }
            const auto destination = room_index(world, destination_room(door.id));
            if (!destination.has_value()) {
                continue;
            }
            graph[index].push_back(Edge{
                *destination,
                door.id,
                arrival_position(world.level.rooms[*destination], room.id),
                edge_cost(room, door.id),
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
        return Result<std::vector<NavStep>>::success({NavStep{std::string(target_room), {}, target, 0}});
    }

    const auto graph = build_graph(world);
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
        path.push_back(NavStep{world.level.rooms[current].id, edge.door, edge.arrival, edge.cost});
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
