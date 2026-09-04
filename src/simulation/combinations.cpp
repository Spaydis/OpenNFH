#include "opennfh/simulation/combinations.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace opennfh::simulation {

namespace {

const content::Combination* find_combination(const WorldState& world, std::string_view name) {
    const auto found = std::find_if(world.level.combinations.begin(), world.level.combinations.end(), [&](const auto& combination) {
        return combination.name == name;
    });
    return found == world.level.combinations.end() ? nullptr : &*found;
}

}  // namespace

bool apply_combination(WorldState& world, std::string_view result_id) {
    const auto* combination = find_combination(world, result_id);
    if (combination == nullptr) {
        return false;
    }

    std::vector<bool> consumed(world.inventory.size(), false);
    std::vector<std::size_t> remove_indices;
    for (const auto& ingredient : combination->ingredients) {
        const auto found = std::find_if(world.inventory.begin(), world.inventory.end(), [&](const std::string& item) {
            const auto index = static_cast<std::size_t>(&item - world.inventory.data());
            return !consumed[index] && item == ingredient.name;
        });
        if (found == world.inventory.end()) {
            return false;
        }
        const auto index = static_cast<std::size_t>(found - world.inventory.begin());
        consumed[index] = true;
        if (ingredient.remove) {
            remove_indices.push_back(index);
        }
    }

    std::sort(remove_indices.rbegin(), remove_indices.rend());
    for (const auto index : remove_indices) {
        world.inventory.erase(world.inventory.begin() + static_cast<std::ptrdiff_t>(index));
    }
    world.flags[std::string(result_id)] = true;
    if (combination->trick) {
        ++world.quotas[std::string(result_id)];
    } else {
        ++world.contents[std::string(result_id)];
    }
    return true;
}

}  // namespace opennfh::simulation
