#include "DiaEditor/MVC/EditorView.h"

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorView::kUniqueId("EditorView");

		EditorView::EditorView()
			: mUISystem(nullptr)
			, mWebUIBridge(nullptr)
			, mDockingLayout(nullptr)
		{
		}

		EditorView::~EditorView()
		{
			Shutdown();
		}

		void EditorView::Initialize(Dia::UI::IUISystem* uiSystem)
		{
			mUISystem = uiSystem;
		}

		void EditorView::Shutdown()
		{
			mUISystem = nullptr;
		}

		void EditorView::RegisterComponent(const char* name, const char* uiPath)
		{
			// Will wire DockingLayout/WebUIBridge when implemented
		}

		Dia::UI::IUISystem* EditorView::GetUISystem()
		{
			return mUISystem;
		}

		WebUIBridge* EditorView::GetWebUIBridge()
		{
			return mWebUIBridge;
		}
	}
}
