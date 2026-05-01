#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaEditor/Command/IEditorCommand.h"

namespace Dia
{
	namespace Editor
	{
		class CompoundCommand : public IEditorCommand
		{
		public:
			CompoundCommand();
			~CompoundCommand() override;

			void AddSubCommand(IEditorCommand* command);

			void Execute() override;
			void Undo() override;
			const char* GetDescription() const override;

		private:
			static const unsigned int kMaxSubCommands = 8;
			Dia::Core::Containers::DynamicArrayC<IEditorCommand*, kMaxSubCommands> mSubCommands;
		};
	}
}
