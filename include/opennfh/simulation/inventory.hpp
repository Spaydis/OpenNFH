#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "opennfh/simulation/world.hpp"

namespace opennfh::simulation {

struct InventoryState {
    std::vector<std::string> items;

    [[nodiscard]] bool contains(std::string_view item) const;
    bool remove_one(std::string_view item);
    void add(std::string item);
};

[[nodiscard]] InventoryState inventory_snapshot(const WorldState& world);

}  // namespace opennfh::simulation
