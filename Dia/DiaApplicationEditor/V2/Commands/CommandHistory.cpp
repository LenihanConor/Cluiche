// CommandHistory.cpp — Undo/redo stack implementation

#include <DiaApplicationEditor/V2/Commands/CommandHistory.h>

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            CommandHistory::CommandHistory()
                : mCount(0u)
                , mCurrentIndex(0u)
                , mSavePointIndex(kSentinelSavePoint)
            {
                for (unsigned int i = 0u; i < kMaxCommands; ++i)
                {
                    mCommands[i] = nullptr;
                }
            }

            CommandHistory::~CommandHistory()
            {
                Clear();
            }

            void CommandHistory::Execute(ICommand* command, ManifestEditorState& doc)
            {
                // Drop any redo tail (commands beyond mCurrentIndex).
                if (mCurrentIndex < mCount)
                {
                    DeleteRange(mCurrentIndex, mCount);
                    mCount = mCurrentIndex;

                    // If the save-point was in the cleared redo tail, it is now
                    // unreachable — mark it as unset so IsAtSavePoint() stays
                    // consistent.
                    if (mSavePointIndex != kSentinelSavePoint &&
                        mSavePointIndex > mCurrentIndex)
                    {
                        mSavePointIndex = kSentinelSavePoint;
                    }
                }

                // If we are already at capacity, evict the oldest command (index 0)
                // and shift everything down by one to make room.
                if (mCount == kMaxCommands)
                {
                    delete mCommands[0];
                    for (unsigned int i = 1u; i < kMaxCommands; ++i)
                    {
                        mCommands[i - 1u] = mCommands[i];
                    }
                    mCommands[kMaxCommands - 1u] = nullptr;
                    --mCount;
                    if (mCurrentIndex > 0u)
                    {
                        --mCurrentIndex;
                    }
                    // Adjust save-point: it referred to the pre-shift slot.
                    if (mSavePointIndex != kSentinelSavePoint)
                    {
                        if (mSavePointIndex == 0u)
                        {
                            // The save-point itself was evicted.
                            mSavePointIndex = kSentinelSavePoint;
                        }
                        else
                        {
                            --mSavePointIndex;
                        }
                    }
                }

                // Execute and store.
                command->Execute(doc);
                mCommands[mCurrentIndex] = command;
                ++mCurrentIndex;
                ++mCount;
            }

            bool CommandHistory::CanUndo() const
            {
                return mCurrentIndex > 0u;
            }

            bool CommandHistory::CanRedo() const
            {
                return mCurrentIndex < mCount;
            }

            void CommandHistory::Undo(ManifestEditorState& doc)
            {
                if (!CanUndo())
                {
                    return;
                }
                --mCurrentIndex;
                mCommands[mCurrentIndex]->Undo(doc);
            }

            void CommandHistory::Redo(ManifestEditorState& doc)
            {
                if (!CanRedo())
                {
                    return;
                }
                mCommands[mCurrentIndex]->Execute(doc);
                ++mCurrentIndex;
            }

            void CommandHistory::Clear()
            {
                DeleteRange(0u, mCount);
                mCount = 0u;
                mCurrentIndex = 0u;
                mSavePointIndex = kSentinelSavePoint;
            }

            unsigned int CommandHistory::GetCurrentIndex() const
            {
                return mCurrentIndex;
            }

            unsigned int CommandHistory::GetCount() const
            {
                return mCount;
            }

            const ICommand* CommandHistory::GetCommand(unsigned int index) const
            {
                if (index >= mCount)
                {
                    return nullptr;
                }
                return mCommands[index];
            }

            void CommandHistory::SetSavePoint()
            {
                mSavePointIndex = mCurrentIndex;
            }

            bool CommandHistory::IsAtSavePoint() const
            {
                return mCurrentIndex == mSavePointIndex;
            }

            // ------------------------------------------------------------------ //
            // Private helpers
            // ------------------------------------------------------------------ //

            void CommandHistory::DeleteRange(unsigned int from, unsigned int to)
            {
                for (unsigned int i = from; i < to; ++i)
                {
                    delete mCommands[i];
                    mCommands[i] = nullptr;
                }
            }

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
