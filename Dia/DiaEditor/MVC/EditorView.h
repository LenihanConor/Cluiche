#pragma once

#include <DiaCore/CRC/StringCRC.h>

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
			static const Dia::Core::StringCRC kUniqueId;

			EditorView();
			~EditorView();

			void Initialize(Dia::UI::IUISystem* uiSystem, EditorViewController* controller);
			void Shutdown();

			void RegisterComponent(const char* name, const char* uiPath);

			Dia::UI::IUISystem* GetUISystem();
			WebUIBridge* GetWebUIBridge();

		private:
			Dia::UI::IUISystem* mUISystem;
			WebUIBridge* mWebUIBridge;
			DockingLayout* mDockingLayout;
		};
	}
}
