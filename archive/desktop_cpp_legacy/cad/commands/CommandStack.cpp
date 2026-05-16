#include "CommandStack.h"

#include "../CadEngine.h"

namespace cad::commands {

bool CommandStack::execute(std::unique_ptr<ICommand> command, CadEngine& engine)
{
    if (command == nullptr || !command->redo(engine)) {
        return false;
    }

    m_redoStack.clear();
    m_undoStack.push_back(std::move(command));
    return true;
}

bool CommandStack::undo(CadEngine& engine)
{
    if (m_undoStack.empty()) {
        return false;
    }

    std::unique_ptr<ICommand> command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->undo(engine);
    m_redoStack.push_back(std::move(command));
    return true;
}

bool CommandStack::redo(CadEngine& engine)
{
    if (m_redoStack.empty()) {
        return false;
    }

    std::unique_ptr<ICommand> command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    if (!command->redo(engine)) {
        return false;
    }
    m_undoStack.push_back(std::move(command));
    return true;
}

bool CommandStack::canUndo() const
{
    return !m_undoStack.empty();
}

bool CommandStack::canRedo() const
{
    return !m_redoStack.empty();
}

void CommandStack::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

} // namespace cad::commands
