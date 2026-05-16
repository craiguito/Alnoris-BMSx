#pragma once

#include "EntityId.h"

namespace cad::core {

struct SelectionState
{
    EntityId primary{};

    void clear()
    {
        primary = {};
    }
};

} // namespace cad::core
