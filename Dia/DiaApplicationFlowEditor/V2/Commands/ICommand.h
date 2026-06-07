// ICommand.h — Abstract command interface for undo/redo system
// Part of DiaApplicationFlowEditor V2

#pragma once

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // Forward declaration — full definition lives in a separate task
            struct ManifestEditorState;

            class ICommand
            {
            public:
                virtual ~ICommand() = default;

                virtual void Execute(ManifestEditorState& doc) = 0;
                virtual void Undo(ManifestEditorState& doc) = 0;
                virtual const char* GetDescription() const = 0;
            };

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
