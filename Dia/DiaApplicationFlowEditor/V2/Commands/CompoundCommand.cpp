// CompoundCommand.cpp — Composite command implementation

#include <DiaApplicationFlowEditor/V2/Commands/CompoundCommand.h>
#include <cstring>

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            CompoundCommand::CompoundCommand()
                : mChildCount(0u)
            {
                for (unsigned int i = 0u; i < kMaxChildren; ++i)
                {
                    mChildren[i] = nullptr;
                }
                mDescription[0] = '\0';
            }

            CompoundCommand::~CompoundCommand()
            {
                for (unsigned int i = 0u; i < mChildCount; ++i)
                {
                    delete mChildren[i];
                    mChildren[i] = nullptr;
                }
                mChildCount = 0u;
            }

            void CompoundCommand::Add(ICommand* child)
            {
                if (child == nullptr || mChildCount >= kMaxChildren)
                {
                    return;
                }
                mChildren[mChildCount] = child;
                ++mChildCount;
            }

            void CompoundCommand::Execute(ManifestEditorState& doc)
            {
                for (unsigned int i = 0u; i < mChildCount; ++i)
                {
                    mChildren[i]->Execute(doc);
                }
            }

            void CompoundCommand::Undo(ManifestEditorState& doc)
            {
                // Reverse order.
                unsigned int i = mChildCount;
                while (i > 0u)
                {
                    --i;
                    mChildren[i]->Undo(doc);
                }
            }

            const char* CompoundCommand::GetDescription() const
            {
                return mDescription;
            }

            void CompoundCommand::SetDescription(const char* desc)
            {
                strcpy_s(mDescription, sizeof(mDescription), desc);
            }

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
