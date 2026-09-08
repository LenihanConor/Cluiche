// CompoundCommand.h — Batches multiple ICommands into a single undo/redo unit
// Part of DiaApplicationFlowEditor V2

#pragma once

#include <DiaApplicationFlowEditor/V2/Commands/ICommand.h>

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // Owns child ICommand* pointers — deletes them on destruction.
            // Execute iterates children forward; Undo iterates children in reverse.
            class CompoundCommand : public ICommand
            {
            public:
                static constexpr unsigned int kMaxChildren = 16;

                CompoundCommand();
                ~CompoundCommand() override;

                // Add a child command. Must be called before Execute.
                // Ownership is transferred — CompoundCommand will delete the child.
                void Add(ICommand* child);

                void Execute(ManifestEditorState& doc) override;
                void Undo(ManifestEditorState& doc) override;
                const char* GetDescription() const override;

                void SetDescription(const char* desc);

            private:
                ICommand* mChildren[kMaxChildren];
                unsigned int mChildCount;
                char mDescription[128];
            };

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
