#include "DiaEditor/Command/CompoundCommand.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Editor
	{
		CompoundCommand::CompoundCommand()
		{
		}

		CompoundCommand::~CompoundCommand()
		{
			for (unsigned int i = 0; i < mSubCommands.Size(); ++i)
			{
				delete mSubCommands[i];
			}
		}

		void CompoundCommand::AddSubCommand(IEditorCommand* command)
		{
			DIA_ASSERT(command != nullptr, "CompoundCommand: sub-command must not be null");
			DIA_ASSERT(!mSubCommands.IsFull(), "CompoundCommand: max sub-command capacity reached");
			mSubCommands.Add(command);
		}

		void CompoundCommand::Execute()
		{
			for (unsigned int i = 0; i < mSubCommands.Size(); ++i)
			{
				mSubCommands[i]->Execute();
			}
		}

		void CompoundCommand::Undo()
		{
			for (int i = static_cast<int>(mSubCommands.Size()) - 1; i >= 0; --i)
			{
				mSubCommands[i]->Undo();
			}
		}

		const char* CompoundCommand::GetDescription() const
		{
			return "CompoundCommand";
		}
	}
}
