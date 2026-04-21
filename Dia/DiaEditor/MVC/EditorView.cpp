#include "DiaEditor/MVC/EditorView.h"

#include "DiaEditor/UI/WebUIBridge.h"
#include "DiaEditor/Layout/DockingLayout.h"

#include <string.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorView::kUniqueId("EditorView");

		static const Dia::Core::StringCRC kReqGetPanels("get_panels");
		static const Dia::Core::StringCRC kReqLoadLayout("load_layout");
		static const Dia::Core::StringCRC kEventSaveLayout("save_layout");

		EditorView::EditorView()
			: mUISystem(nullptr)
			, mWebUIBridge(nullptr)
			, mDockingLayout(nullptr)
		{
			mLayoutPath[0] = '\0';
		}

		EditorView::~EditorView()
		{
			Shutdown();
		}

		void EditorView::Initialize(Dia::UI::IUISystem* uiSystem, EditorViewController* controller)
		{
			mUISystem = uiSystem;

			mDockingLayout = new DockingLayout();
			mDockingLayout->RegisterPanel("Home", "dia://editor/home/index.html");

			mWebUIBridge = new WebUIBridge(uiSystem);
			mWebUIBridge->Initialize(controller);

			RegisterBuiltInRequestHandlers();
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

		void EditorView::SetLayoutPath(const char* path)
		{
			if (path == nullptr)
			{
				mLayoutPath[0] = '\0';
				return;
			}
			strncpy_s(mLayoutPath, kMaxLayoutPathLength, path, _TRUNCATE);
		}

		const char* EditorView::GetLayoutPath() const
		{
			return mLayoutPath;
		}

		void EditorView::LoadLayoutFromDisk()
		{
			if (mDockingLayout == nullptr || mLayoutPath[0] == '\0')
				return;
			mDockingLayout->LoadFromDisk(mLayoutPath);
		}

		void EditorView::SaveLayoutToDisk() const
		{
			if (mDockingLayout == nullptr || mLayoutPath[0] == '\0')
				return;
			mDockingLayout->SaveToDisk(mLayoutPath);
		}

		Dia::UI::IUISystem* EditorView::GetUISystem()
		{
			return mUISystem;
		}

		WebUIBridge* EditorView::GetWebUIBridge()
		{
			return mWebUIBridge;
		}

		DockingLayout* EditorView::GetDockingLayout()
		{
			return mDockingLayout;
		}

		void EditorView::RegisterBuiltInRequestHandlers()
		{
			if (mWebUIBridge == nullptr || mDockingLayout == nullptr)
				return;

			mWebUIBridge->RegisterRequestHandler(kReqGetPanels,
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					result["panels"] = Json::arrayValue;
					for (unsigned int i = 0; i < mDockingLayout->GetPanelCount(); ++i)
					{
						const DockingLayout::PanelInfo& info = mDockingLayout->GetPanel(i);
						Json::Value panel;
						panel["name"] = info.name;
						panel["uiPath"] = info.uiPath;
						panel["visible"] = info.visible;
						result["panels"].append(panel);
					}
					return result;
				});

			mWebUIBridge->RegisterRequestHandler(kReqLoadLayout,
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					mDockingLayout->Serialize(result);
					return result;
				});

			mWebUIBridge->RegisterEventHandler(kEventSaveLayout,
				[this](const Json::Value& data)
				{
					const Json::Value& layout = data.isMember("layout") ? data["layout"] : data;
					Json::Value copy = layout;
					mDockingLayout->ValidateLayout(copy);
					mDockingLayout->Deserialize(copy);
					SaveLayoutToDisk();
				});
		}
	}
}
