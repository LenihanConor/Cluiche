#include "DiaEditor/MVC/EditorView.h"

#include "DiaEditor/UI/WebUIBridge.h"
#include "DiaEditor/Layout/DockingLayout.h"
#include "DiaEditor/MVC/EditorViewController.h"

#include <string.h>
#include <time.h>
#include <stdio.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorView::kUniqueId("EditorView");

		static const Dia::Core::StringCRC kReqGetPanels("get_panels");
		static const Dia::Core::StringCRC kReqGetCommands("get_commands");
		static const Dia::Core::StringCRC kReqLoadLayout("load_layout");
		static const Dia::Core::StringCRC kEventSaveLayout("save_layout");
		static const Dia::Core::StringCRC kEventExecuteCommand("execute_command");
		static const char* kTopicConsoleEntries = "console_entries";

		EditorView::EditorView()
			: mUISystem(nullptr)
			, mWebUIBridge(nullptr)
			, mDockingLayout(nullptr)
			, mController(nullptr)
			, mNextConsoleEntryId(1)
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
			mController = controller;

			mDockingLayout = new DockingLayout();
			mDockingLayout->RegisterPanel("Home", "dia://editor/home/index.html");
			mDockingLayout->RegisterPanel("Output Console", "dia://editor/outputconsole/index.html");

			mWebUIBridge = new WebUIBridge(uiSystem);
			mWebUIBridge->Initialize(controller);

			RegisterBuiltInRequestHandlers();
			RegisterBuiltInCommands();
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
			mController = nullptr;
		}

		void EditorView::RegisterComponent(const char* name, const char* uiPath)
		{
			if (mDockingLayout)
				mDockingLayout->RegisterPanel(name, uiPath);
		}

		void EditorView::RegisterCommand(const char* id, const char* label)
		{
			if (id == nullptr || label == nullptr)
				return;
			if (mCommands.IsFull())
				return;

			CommandInfo info;
			strncpy_s(info.id, sizeof(info.id), id, _TRUNCATE);
			strncpy_s(info.label, sizeof(info.label), label, _TRUNCATE);
			mCommands.Add(info);
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

		void EditorView::PushConsoleEntry(const char* level, const char* message)
		{
			if (mWebUIBridge == nullptr || level == nullptr || message == nullptr)
				return;

			time_t now = time(nullptr);
			struct tm local;
			localtime_s(&local, &now);
			char timestamp[16];
			snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d",
				local.tm_hour, local.tm_min, local.tm_sec);

			Json::Value entry;
			entry["id"] = mNextConsoleEntryId++;
			entry["level"] = level;
			entry["message"] = message;
			entry["timestamp"] = timestamp;

			mWebUIBridge->NotifyUIDataChanged(kTopicConsoleEntries, entry);
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

			mWebUIBridge->RegisterRequestHandler(kReqGetCommands,
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					result["commands"] = Json::arrayValue;
					for (unsigned int i = 0; i < mCommands.Size(); ++i)
					{
						Json::Value cmd;
						cmd["id"] = mCommands[i].id;
						cmd["label"] = mCommands[i].label;
						result["commands"].append(cmd);
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

			mWebUIBridge->RegisterEventHandler(kEventExecuteCommand,
				[this](const Json::Value& data)
				{
					if (mController == nullptr)
						return;
					const std::string commandId = data.get("commandId", "").asString();
					if (commandId.empty())
						return;
					Dia::Core::StringCRC cmd(commandId.c_str());
					mController->OnUIEvent(cmd, data);

					char msg[128];
					snprintf(msg, sizeof(msg), "Executed command: %s", commandId.c_str());
					PushConsoleEntry("info", msg);
				});
		}

		void EditorView::RegisterBuiltInCommands()
		{
			RegisterCommand("undo", "Edit: Undo");
			RegisterCommand("redo", "Edit: Redo");
			RegisterCommand("log.test", "Debug: Emit Test Log Entry");
		}
	}
}
