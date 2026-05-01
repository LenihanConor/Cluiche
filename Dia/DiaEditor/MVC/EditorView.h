#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>

namespace Dia
{
	namespace UI
	{
		class IUISystem;
	}

	namespace Editor
	{
		class WebUIBridge;
		class DockingLayout;

		class EditorViewController;

		class EditorView
		{
		public:
			struct CommandInfo
			{
				Dia::Core::Containers::String64 id;
				Dia::Core::Containers::String128 label;
			};

			static const Dia::Core::StringCRC kUniqueId;

			EditorView();
			~EditorView();

			void Initialize(Dia::UI::IUISystem* uiSystem, EditorViewController* controller);
			void Shutdown();

			void RegisterComponent(const char* name, const char* uiPath);
			void UnregisterComponent(const char* name);
			void SetComponentVisible(const char* name, bool visible);
			void NotifyPanelsChanged();
			void RegisterCommand(const char* id, const char* label);

			void SetLayoutPath(const char* path);
			const char* GetLayoutPath() const;

			void LoadLayoutFromDisk();
			void SaveLayoutToDisk() const;

			void PushConsoleEntry(const char* level, const char* message, const char* source = "editor");

			Dia::UI::IUISystem* GetUISystem();
			WebUIBridge* GetWebUIBridge();
			DockingLayout* GetDockingLayout();

		private:
			void RegisterBuiltInRequestHandlers();
			void RegisterBuiltInCommands();

			Dia::UI::IUISystem* mUISystem;
			WebUIBridge* mWebUIBridge;
			DockingLayout* mDockingLayout;
			EditorViewController* mController;
			bool mInitialized;

			static const unsigned int kMaxLayoutPathLength = 260;
			char mLayoutPath[kMaxLayoutPathLength];

			static const unsigned int kMaxCommands = 64;
			Dia::Core::Containers::DynamicArrayC<CommandInfo, kMaxCommands> mCommands;

			unsigned int mNextConsoleEntryId;
		};
	}
}
