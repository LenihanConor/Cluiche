#pragma once

#include <DiaLogger/ISink.h>

namespace Dia
{
	namespace Editor
	{
		class EditorView;

		class EditorConsoleSink : public Dia::Logger::ISink
		{
		public:
			explicit EditorConsoleSink(EditorView* view);

			void OnLogEntry(const Dia::Logger::LogEntry& entry) override;
			const char* GetName() const override { return "EditorConsole"; }

		private:
			EditorView* mView;
		};
	}
}
