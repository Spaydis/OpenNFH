#include <cassert>

#include "opennfh/presentation/viewport.hpp"

int main() {
    const opennfh::presentation::ViewportTransform transform =
        opennfh::presentation::make_viewport({{948, 600}, 1920, 1080, true});

    const auto top_left = transform.to_screen({0, 0});
    assert(top_left.x == 486);
    assert(top_left.y == 240);
    const auto bottom_right = transform.to_screen({948, 600});
    assert(bottom_right.x == 1434);
    assert(bottom_right.y == 840);
    const auto logical = transform.to_logical(top_left);
    assert(logical.x == 0);
    assert(logical.y == 0);

    const auto fit = opennfh::presentation::make_viewport({{948, 600}, 1280, 720, false});
    assert(fit.offset.x == 71);
    assert(fit.offset.y == 0);
    const auto large = opennfh::presentation::make_viewport({{948, 600}, 1920, 1080, false});
    assert(large.offset.x == 107);
    assert(large.offset.y == 0);
    return 0;
}
