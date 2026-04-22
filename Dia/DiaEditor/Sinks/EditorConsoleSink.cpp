#include "DiaEditor/Sinks/EditorConsoleSink.h"

#include "DiaEditor/MVC/EditorView.h"
#include <DiaLogger/LogLevel.h>

namespace Dia
{
	namespace Editor
	{
		EditorConsoleSink::EditorConsoleSink(EditorView* view)
			: mView(view)
		{
		}

		void EditorConsoleSink::OnLogEntry(const Dia::Logger::LogEntry& entry)
		{
			if (mView == nullptr)
				return;

			mView->PushConsoleEntry(
				Dia::Logger::LogLevelToString(entry.level),
				entry.message,
				"editor");
		}
	}
}
