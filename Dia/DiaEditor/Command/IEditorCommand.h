#pragma once

namespace Dia
{
	namespace Editor
	{
		class IEditorCommand
		{
		public:
			virtual ~IEditorCommand() = default;
			virtual void Execute() = 0;
			virtual void Undo() = 0;
			virtual const char* GetDescription() const = 0;
		};
	}
}
