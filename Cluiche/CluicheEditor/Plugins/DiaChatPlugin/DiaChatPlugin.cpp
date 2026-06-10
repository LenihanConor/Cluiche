#include "Plugins/DiaChatPlugin/DiaChatPlugin.h"
#include "Plugins/DiaChatPlugin/ChatPanelBridge.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

const Dia::Core::StringCRC CluicheEditor::DiaChatPlugin::kPluginId("DiaChatPlugin");

namespace CluicheEditor
{
	DiaChatPlugin::DiaChatPlugin()
		: mBridge(nullptr)
		, mWebBridge(nullptr)
	{}

	void DiaChatPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
	{
		DIA_LOG_INFO("Chat", "DiaChatPlugin: OnLoad");

		mWebBridge = context.mBridge;

		if (mBridge == nullptr)
			mBridge = new ChatPanelBridge(mWebBridge);
		else
			mBridge->Initialize(mWebBridge);

		if (mWebBridge != nullptr)
		{
			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.send_message"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.send_message received");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.set_backend"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.set_backend received");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.set_context_mode"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.set_context_mode received");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.add_context_file"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.add_context_file received");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.clear_history"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.clear_history received");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.confirm_response"),
				[](const Json::Value& /*data*/)
				{
					DIA_LOG_INFO("Chat", "chat.confirm_response received");
				});
		}
	}

	void DiaChatPlugin::OnUnload()
	{
		DIA_LOG_INFO("Chat", "DiaChatPlugin: OnUnload");

		if (mWebBridge != nullptr)
		{
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.send_message"));
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.set_backend"));
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.set_context_mode"));
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.add_context_file"));
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.clear_history"));
			mWebBridge->UnregisterEventHandler(Dia::Core::StringCRC("chat.confirm_response"));

			mWebBridge = nullptr;
		}

		delete mBridge;
		mBridge = nullptr;
	}

	void DiaChatPlugin::OnUpdate(float deltaTime)
	{
		if (mBridge != nullptr)
			mBridge->DoUpdate(deltaTime);
	}
}

using namespace CluicheEditor;

REGISTER_EDITOR_PLUGIN(DiaChatPlugin, "DiaChatPlugin")
