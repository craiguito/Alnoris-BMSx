#pragma once

#include "ICommand.h"

#include <memory>
#include <vector>

namespace cad {
class CadEngine;
}

namespace cad::commands {

class CommandStack
{
public:
    bool execute(std::unique_ptr<ICommand> command, CadEngine& engine);
    bool undo(CadEngine& engine);
    bool redo(CadEngine& engine);

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

private:
    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
};

} // namespace cad::commands
