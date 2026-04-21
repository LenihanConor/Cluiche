#include "DiaEditor/MVC/EditorView.h"

#include "DiaEditor/UI/WebUIBridge.h"
#include "DiaEditor/Layout/DockingLayout.h"

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

		void EditorView::Initialize(Dia::UI::IUISystem* uiSystem, EditorViewController* controller)
		{
			mUISystem = uiSystem;

			mDockingLayout = new DockingLayout();

			mWebUIBridge = new WebUIBridge(uiSystem);
			mWebUIBridge->Initialize(controller);
		}

		void EditorView::Shutdown()
		{
			if (mWebUIBridge)
			{
				delete mWebUIBridge;
				mWebUIBridge = nullptr;
			}
			if (mDockingLayout)
			{
				delete mDockingLayout;
				mDockingLayout = nullptr;
			}
			mUISystem = nullptr;
		}

		void EditorView::RegisterComponent(const char* name, const char* uiPath)
		{
			if (mDockingLayout)
				mDockingLayout->RegisterPanel(name, uiPath);
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
