#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaEditor/Command/IEditorCommand.h"
#include "DiaEditor/Command/CompoundCommand.h"

namespace Dia
{
	namespace Editor
	{
		class CommandHistory
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			CommandHistory();
			~CommandHistory();

			void ExecuteCommand(IEditorCommand* command);
			void Undo();
			void Redo();

			void BeginCompound();
			void EndCompound();

			void MarkSavePoint();
			bool IsAtSavePoint() const;

			bool CanUndo() const;
			bool CanRedo() const;

			void Clear();

		private:
			void ClearRedoStack();
			void PushToUndoStack(IEditorCommand* command);

			static const unsigned int kMaxStackDepth = 128;

			Dia::Core::Containers::DynamicArrayC<IEditorCommand*, kMaxStackDepth> mUndoStack;
			Dia::Core::Containers::DynamicArrayC<IEditorCommand*, kMaxStackDepth> mRedoStack;

			CompoundCommand* mOpenCompound;
			int mSavePointIndex;
		};
	}
}
