// CommandHistory.h — Undo/redo stack for the ApplicationFlow editor
// Part of DiaApplicationFlowEditor V2

#pragma once

#include <DiaApplicationEditor/V2/Commands/ICommand.h>

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // Owns ICommand* pointers. Deletes evicted and cleared commands.
            // Ring behaviour on overflow: oldest command is dropped (shift).
            class CommandHistory
            {
            public:
                static constexpr unsigned int kMaxCommands = 100;

                CommandHistory();
                ~CommandHistory();

                // Execute cmd->Execute(doc), store the command, clear redo tail.
                void Execute(ICommand* command, ManifestEditorState& doc);

                bool CanUndo() const;
                bool CanRedo() const;

                void Undo(ManifestEditorState& doc);
                void Redo(ManifestEditorState& doc);

                // Delete all commands and reset indices (including save-point).
                void Clear();

                unsigned int GetCurrentIndex() const;
                unsigned int GetCount() const;

                // Returns nullptr for out-of-range index.
                const ICommand* GetCommand(unsigned int index) const;

                // Save-point: tracks where the last file-save occurred.
                void SetSavePoint();
                bool IsAtSavePoint() const;

            private:
                // Deletes and nulls out commands in [from, to).
                void DeleteRange(unsigned int from, unsigned int to);

                ICommand* mCommands[kMaxCommands];

                // Number of valid commands stored (0..kMaxCommands).
                unsigned int mCount;

                // Index of the next slot to write into (points past the last
                // executed command). Range: 0..mCount.
                unsigned int mCurrentIndex;

                // Index recorded at the last SetSavePoint() call.
                // kSentinelSavePoint means no save-point has been set yet.
                static constexpr unsigned int kSentinelSavePoint = ~0u;
                unsigned int mSavePointIndex;
            };

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
