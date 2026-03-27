#pragma once

#include <cstdint>
#include <functional>

namespace cad::core {

struct EntityId
{
    std::uint64_t value = 0;

    [[nodiscard]] bool isValid() const { return value != 0; }

    friend bool operator==(EntityId lhs, EntityId rhs) { return lhs.value == rhs.value; }
    friend bool operator!=(EntityId lhs, EntityId rhs) { return !(lhs == rhs); }
};

struct EntityIdHash
{
    std::size_t operator()(EntityId id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace cad::core
