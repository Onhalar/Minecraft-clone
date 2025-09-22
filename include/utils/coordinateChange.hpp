#ifndef COORDINATE_SYSTEM_SWITCHEROO
#define COORDINATE_SYSTEM_SWITCHEROO

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <type_traits>

namespace Coordinates {

    struct coordinateSystem {
        glm::vec3 value;
        constexpr coordinateSystem(glm::vec3 value): value(value) {}
        constexpr operator glm::vec3() const { return value; }

        template<typename T>
        constexpr operator T() const {
            static_assert(std::is_same_v<coordinateSystem, T>, "Cannot convert to non-coordinate-system type");
            return T(glm::vec3(value.x, value.z, value.y));
        }
    };

    // native format Z -> up and down (used by system)
    // do NOT try to convert to itself it WILL swap Z and Y
    using Y_up = coordinateSystem;

    // standard format Y -> up and down
    // do NOT try to convert to itself it WILL swap Z and Y
    using Z_up = coordinateSystem;
}

#endif // COORDINATE_SYSTEM_SWITCHEROO