#pragma once

namespace cad {
class CadEngine;
}

namespace cad::commands {

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual bool redo(CadEngine& engine) = 0;
    virtual void undo(CadEngine& engine) = 0;
};

} // namespace cad::commands
