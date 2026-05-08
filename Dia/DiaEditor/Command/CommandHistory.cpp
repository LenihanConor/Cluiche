#include "DiaEditor/Command/CommandHistory.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC CommandHistory::kUniqueId("CommandHistory");

		CommandHistory::CommandHistory()
			: mOpenCompound(nullptr)
			, mSavePointIndex(-1)
		{
		}

		CommandHistory::~CommandHistory()
		{
			Clear();
		}

		void CommandHistory::ExecuteCommand(IEditorCommand* command)
		{
			DIA_ASSERT(command != nullptr, "CommandHistory: command must not be null");

			if (mOpenCompound != nullptr)
			{
				mOpenCompound->AddSubCommand(command);
				command->Execute();
				return;
			}

			command->Execute();
			PushToUndoStack(command);
			ClearRedoStack();
		}

		void CommandHistory::Undo()
		{
			if (!CanUndo())
				return;

			IEditorCommand* command = mUndoStack.Back();
			mUndoStack.Remove();

			command->Undo();
			mRedoStack.Add(command);
		}

		void CommandHistory::Redo()
		{
			if (!CanRedo())
				return;

			IEditorCommand* command = mRedoStack.Back();
			mRedoStack.Remove();

			command->Execute();
			mUndoStack.Add(command);
		}

		void CommandHistory::BeginCompound()
		{
			DIA_ASSERT(mOpenCompound == nullptr, "CommandHistory: compound already open");
			mOpenCompound = new CompoundCommand();
		}

		void CommandHistory::EndCompound()
		{
			DIA_ASSERT(mOpenCompound != nullptr, "CommandHistory: no compound open");
			CompoundCommand* compound = mOpenCompound;
			mOpenCompound = nullptr;
			PushToUndoStack(compound);
			ClearRedoStack();
		}

		void CommandHistory::MarkSavePoint()
		{
			mSavePointIndex = static_cast<int>(mUndoStack.Size());
		}

		bool CommandHistory::IsAtSavePoint() const
		{
			return mSavePointIndex == static_cast<int>(mUndoStack.Size());
		}

		bool CommandHistory::CanUndo() const { return !mUndoStack.IsEmpty(); }
		bool CommandHistory::CanRedo() const { return !mRedoStack.IsEmpty(); }

		void CommandHistory::Clear()
		{
			if (mOpenCompound != nullptr)
			{
				delete mOpenCompound;
				mOpenCompound = nullptr;
			}
			for (unsigned int i = 0; i < mUndoStack.Size(); ++i)
				delete mUndoStack[i];
			mUndoStack.RemoveAll();

			for (unsigned int i = 0; i < mRedoStack.Size(); ++i)
				delete mRedoStack[i];
			mRedoStack.RemoveAll();
		}

		void CommandHistory::ClearRedoStack()
		{
			for (unsigned int i = 0; i < mRedoStack.Size(); ++i)
				delete mRedoStack[i];
			mRedoStack.RemoveAll();

			if (mSavePointIndex > static_cast<int>(mUndoStack.Size()))
				mSavePointIndex = -1;
		}

		void CommandHistory::PushToUndoStack(IEditorCommand* command)
		{
			if (mUndoStack.IsFull())
			{
				delete mUndoStack[0];
				mUndoStack.RemoveAt(0);
				if (mSavePointIndex > 0)
					--mSavePointIndex;
				else
					mSavePointIndex = -1;
			}
			mUndoStack.Add(command);
		}
	}
}
