#include "opennfh/simulation/inventory.hpp"

#include <algorithm>
#include <utility>

namespace opennfh::simulation {

bool InventoryState::contains(std::string_view item) const {
    return std::find(items.begin(), items.end(), item) != items.end();
}

bool InventoryState::remove_one(std::string_view item) {
    const auto found = std::find(items.begin(), items.end(), item);
    if (found == items.end()) {
        return false;
    }
    items.erase(found);
    return true;
}

void InventoryState::add(std::string item) {
    items.push_back(std::move(item));
}

InventoryState inventory_snapshot(const WorldState& world) {
    return InventoryState{world.inventory};
}

}  // namespace opennfh::simulation
