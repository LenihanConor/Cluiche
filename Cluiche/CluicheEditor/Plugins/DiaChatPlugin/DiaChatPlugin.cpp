#include "Plugins/DiaChatPlugin/DiaChatPlugin.h"
#include "Plugins/DiaChatPlugin/ChatPanelBridge.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionQueueService.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaPython/Lifecycle/Lifecycle.h>
#include <DiaPython/Module/Module.h>
#include <DiaPython/ScriptExecution/Script.h>
#include <DiaPython/TypeConversion/Conversion.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <string>

const Dia::Core::StringCRC CluicheEditor::DiaChatPlugin::kPluginId("DiaChatPlugin");

namespace CluicheEditor
{
	// ---------------------------------------------------------------------------
	// JSON helpers
	// ---------------------------------------------------------------------------

	static std::string SerializeJson(const Json::Value& v)
	{
		Json::StreamWriterBuilder b;
		b["indentation"] = "";
		return Json::writeString(b, v);
	}

	static Json::Value ParseJson(const char* raw)
	{
		Json::Value result;
		if (!raw || raw[0] == '\0') return result;
		Json::CharReaderBuilder rb;
		std::string err;
		std::unique_ptr<Json::CharReader> reader(rb.newCharReader());
		reader->parse(raw, raw + strlen(raw), &result, &err);
		return result;
	}

	// ---------------------------------------------------------------------------
	// DiaChatPlugin implementation
	// ---------------------------------------------------------------------------

	DiaChatPlugin::DiaChatPlugin()
	{}

	DiaChatPlugin::~DiaChatPlugin()
	{
		if (mSendThread.joinable())
			mSendThread.join();
	}

	void DiaChatPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
	{
		DIA_LOG_INFO("Chat", "DiaChatPlugin: OnLoad");

		mWebBridge = context.mBridge;

		// Grab the action queue from the service locator (registered by PluginLoaderModule).
		if (context.mServices != nullptr)
		{
			auto* queueSvc = context.mServices->GetService<Dia::Editor::EditorActionQueueService>();
			if (queueSvc != nullptr)
				mActionQueue = queueSvc->GetQueue();
		}

		if (mBridge == nullptr)
			mBridge = new ChatPanelBridge(mWebBridge);
		else
			mBridge->Initialize(mWebBridge);

		// ------------------------------------------------------------------
		// 1. Ensure Python is initialized and scripts/ is on sys.path.
		// ------------------------------------------------------------------
		if (!Dia::Python::IsInitialized())
		{
			bool ok = Dia::Python::Initialize("External/Python311/", "scripts/", false);
			if (!ok)
			{
				DIA_LOG_ERROR("Chat", "DiaChatPlugin: Python initialization failed — chat disabled");
				RegisterStubHandlers();
				return;
			}
		}
		else
		{
			// Python already initialized (e.g. by DiaPythonConsolePlugin) —
			// make sure scripts/ is on sys.path so dia_chat.py can be imported.
			Dia::Python::ExecuteString(
				"import sys; "
				"('scripts/' not in sys.path) and sys.path.append('scripts/')");
		}

		// ------------------------------------------------------------------
		// 2. Register the dia_chat C++ module (callbacks Python can call).
		// ------------------------------------------------------------------
		RegisterPythonModule();

		// ------------------------------------------------------------------
		// 3. Load dia_chat.py (defines module-level functions + classes).
		// ------------------------------------------------------------------
		int rc = Dia::Python::ExecuteScript("scripts/dia_chat.py");
		if (rc != 0)
		{
			DIA_LOG_ERROR("Chat", "DiaChatPlugin: ExecuteScript(dia_chat.py) failed rc=%d", rc);
			RegisterStubHandlers();
			return;
		}

		// ------------------------------------------------------------------
		// 4. Call dia_chat.initialize() passing all C++ callbacks as Python
		//    callables that were registered in step 2 under dia_chat_bridge.*
		//    dia_chat.py will import them to set up the ChatOrchestrator.
		// ------------------------------------------------------------------
		const char* projectPath = (context.mProjectPath != nullptr) ? context.mProjectPath : "";
		// Define Python-level wrappers that convert dict args to JSON strings before
		// forwarding to the C++ callbacks (which only handle str args).
		Dia::Python::ExecuteString(
			"import dia_chat_bridge, json\n"
			"def _notify_bridge_py(topic, payload):\n"
			"    dia_chat_bridge._notify_bridge_raw(topic, json.dumps(payload))\n"
			"dia_chat_bridge.notify_bridge = _notify_bridge_py\n"
			"_ea_raw = dia_chat_bridge.execute_action\n"
			"def _execute_action_py(name, params):\n"
			"    raw = _ea_raw(name, json.dumps(params) if isinstance(params, dict) else str(params))\n"
			"    return json.loads(raw) if isinstance(raw, str) else raw\n"
			"dia_chat_bridge.execute_action = _execute_action_py\n"
			"_cc_raw = dia_chat_bridge.confirm_callback\n"
			"def _confirm_callback_py(call_id, fn, params, description):\n"
			"    _cc_raw(call_id, fn, json.dumps(params) if isinstance(params, dict) else str(params), description)\n"
			"dia_chat_bridge.confirm_callback = _confirm_callback_py\n");

		char initCall[512];
		snprintf(initCall, sizeof(initCall),
			"import dia_chat, dia_chat_bridge; "
			"dia_chat.initialize("
				"dia_chat_bridge.token_callback,"
				"dia_chat_bridge.manifest_getter,"
				"dia_chat_bridge.execute_action,"
				"dia_chat_bridge.confirm_callback,"
				"notify_bridge=dia_chat_bridge.notify_bridge,"
				"ai_context_dir='assets/ai_context/',"
				"token_budget=4096,"
				"project_path='%s'"
			")",
			projectPath);

		rc = Dia::Python::ExecuteString(initCall);
		if (rc != 0)
			DIA_LOG_WARNING("Chat", "DiaChatPlugin: dia_chat.initialize() failed rc=%d", rc);

		// ------------------------------------------------------------------
		// 5. Probe the default backend (Ollama) and push status to UI.
		// ------------------------------------------------------------------
		Dia::Python::ExecuteString(
			"import dia_chat; dia_chat.set_backend('ollama', 'llama3.2')");

		// ------------------------------------------------------------------
		// 6. Register WebUIBridge event handlers.
		// ------------------------------------------------------------------
		if (mWebBridge != nullptr)
		{
			ChatPanelBridge* bridge = mBridge;

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.send_message"),
				[this](const Json::Value& data)
				{
					DIA_LOG_INFO("Chat", "chat.send_message received");
					std::string text = data.get("text", "").asString();
					std::string mode = data.get("context_mode", "full_context").asString();
						DispatchSendMessage(text, mode);
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.set_backend"),
				[](const Json::Value& data)
				{
					std::string backend = data.get("backend", "ollama").asString();
					std::string model   = data.get("model", "llama3.2").asString();
					char cmd[256];
					snprintf(cmd, sizeof(cmd), "import dia_chat; dia_chat.set_backend('%s', '%s')",
						backend.c_str(), model.c_str());
					Dia::Python::ExecuteString(cmd);
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.set_context_mode"),
				[](const Json::Value& data)
				{
					std::string mode = data.get("mode", "full_context").asString();
					char cmd[128];
					snprintf(cmd, sizeof(cmd), "import dia_chat; dia_chat.set_context_mode('%s')", mode.c_str());
					Dia::Python::ExecuteString(cmd);
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.add_context_file"),
				[](const Json::Value& data)
				{
					std::string path = data.get("path", "").asString();
					char cmd[512];
					snprintf(cmd, sizeof(cmd), "import dia_chat; dia_chat.add_context_file('%s')", path.c_str());
					Dia::Python::ExecuteString(cmd);
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.clear_history"),
				[](const Json::Value& /*data*/)
				{
					Dia::Python::ExecuteString("import dia_chat; dia_chat.clear_history()");
				});

			mWebBridge->RegisterEventHandler(
				Dia::Core::StringCRC("chat.confirm_response"),
				[](const Json::Value& data)
				{
					std::string callId    = data.get("call_id", "").asString();
					bool        confirmed = data.get("confirmed", false).asBool();
					char cmd[256];
					snprintf(cmd, sizeof(cmd),
						"import dia_chat; dia_chat.on_confirm_response('%s', %s)",
						callId.c_str(), confirmed ? "True" : "False");
					Dia::Python::ExecuteString(cmd);
				});
		}
	}

	void DiaChatPlugin::RegisterPythonModule()
	{
		// Create the dia_chat_bridge module — exposes C++ callbacks into Python.
		// GetModule is safe to call even if the module already exists (plugin reload).
		Dia::Python::Module* mod = Dia::Python::GetModule("dia_chat_bridge");
		if (mod == nullptr)
			mod = Dia::Python::CreateModule("dia_chat_bridge");
		if (mod == nullptr)
		{
			DIA_LOG_ERROR("Chat", "DiaChatPlugin: failed to create dia_chat_bridge module");
			return;
		}

		ChatPanelBridge* bridge = mBridge;
		Dia::Editor::EditorActionQueue* queue = mActionQueue;

		// token_callback(text: str, done: bool)
		Dia::Python::AddFunction(mod, "token_callback",
			[bridge](const Dia::Python::PythonArgs& args) -> Dia::Python::PythonObject
			{
				const char* text = (args.GetCount() > 0) ? Dia::Python::ToString(args.GetArg(0)) : "";
				bool done = (args.GetCount() > 1) ? Dia::Python::ToBool(args.GetArg(1)) : false;
				if (bridge) bridge->OnTokenChunk(text, done);
				return Dia::Python::PythonObject();
			},
			"token_callback(text, done) -> None");

		// manifest_getter() -> str (JSON)  — returns the DiaEditorAPI manifest as JSON string
		Dia::Python::AddFunction(mod, "manifest_getter",
			[](const Dia::Python::PythonArgs& /*args*/) -> Dia::Python::PythonObject
			{
				// The dia_editor module already exists with all registered actions.
				// Return None here — dia_chat.py handles the None case gracefully
				// (knowledge-only mode). Full manifest integration is Phase 3.
				return Dia::Python::PythonObject();  // None
			},
			"manifest_getter() -> None (manifest integration deferred)");

		// execute_action(name: str, params_json: str) -> str (JSON result)
		Dia::Python::AddFunction(mod, "execute_action",
			[queue](const Dia::Python::PythonArgs& args) -> Dia::Python::PythonObject
			{
				if (queue == nullptr || args.GetCount() < 2)
				{
					Json::Value err;
					err["success"] = false;
					err["reason"]  = "no_queue";
					return Dia::Python::ToPython(SerializeJson(err).c_str());
				}

				const char* nameStr   = Dia::Python::ToString(args.GetArg(0));
				const char* paramsStr = Dia::Python::ToString(args.GetArg(1));

				Dia::Core::StringCRC actionName(nameStr ? nameStr : "");
				Json::Value params = ParseJson(paramsStr);

				Json::Value result = queue->DispatchAndWait(actionName, params);
				return Dia::Python::ToPython(SerializeJson(result).c_str());
			},
			"execute_action(name, params_json) -> result_json");

		// confirm_callback(call_id, fn, params_json, description)
		Dia::Python::AddFunction(mod, "confirm_callback",
			[bridge](const Dia::Python::PythonArgs& args) -> Dia::Python::PythonObject
			{
				if (bridge == nullptr) return Dia::Python::PythonObject();
				const char* callId      = (args.GetCount() > 0) ? Dia::Python::ToString(args.GetArg(0)) : "";
				const char* fn          = (args.GetCount() > 1) ? Dia::Python::ToString(args.GetArg(1)) : "";
				const char* paramsJson  = (args.GetCount() > 2) ? Dia::Python::ToString(args.GetArg(2)) : "{}";
				const char* description = (args.GetCount() > 3) ? Dia::Python::ToString(args.GetArg(3)) : "";
				bridge->OnConfirmRequired(callId, fn, ParseJson(paramsJson), description);
				return Dia::Python::PythonObject();
			},
			"confirm_callback(call_id, fn, params_json, description) -> None");

		// _notify_bridge_raw(topic: str, payload_json: str) — C++ side
		// dia_chat.py calls notify_bridge(topic, dict); a Python wrapper (defined in the
		// init ExecuteString below) does json.dumps before forwarding here.
		Dia::Python::AddFunction(mod, "_notify_bridge_raw",
			[bridge](const Dia::Python::PythonArgs& args) -> Dia::Python::PythonObject
			{
				if (bridge == nullptr || args.GetCount() < 2) return Dia::Python::PythonObject();
				const char* topic      = Dia::Python::ToString(args.GetArg(0));
				const char* payloadStr = Dia::Python::ToString(args.GetArg(1));
				Json::Value payload = ParseJson(payloadStr);

				// Route to the appropriate ChatPanelBridge method.
				if (topic == nullptr) return Dia::Python::PythonObject();

				if (strcmp(topic, "chat.error") == 0)
					bridge->OnChatError(SerializeJson(payload).c_str());
				else if (strcmp(topic, "chat.backend_status") == 0)
				{
					std::string backend = payload.get("backend", "").asString();
					std::string model   = payload.get("model", "").asString();
					bool available      = payload.get("available", false).asBool();

					// Error-style backend_status (status: error) has no backend/model fields.
					if (payload.isMember("status") && payload["status"].asString() == "error")
					{
						bridge->OnBackendStatus("", "", false);
						// Also push as a chat.error so the UI shows the banner.
						bridge->OnChatError(SerializeJson(payload).c_str());
					}
					else
					{
						bridge->OnBackendStatus(backend.c_str(), model.c_str(), available);
					}
				}

				return Dia::Python::PythonObject();
			},
			"notify_bridge(topic, payload_json) -> None");
	}

	void DiaChatPlugin::RegisterStubHandlers()
	{
		// Fallback when Python init fails — just log; UI stays silent.
		if (mWebBridge == nullptr) return;
		mWebBridge->RegisterEventHandler(Dia::Core::StringCRC("chat.send_message"),
			[this](const Json::Value& /*d*/) {
				DIA_LOG_WARNING("Chat", "chat.send_message: Python not available");
				if (mBridge) mBridge->OnChatError("{\"message\":\"Python not available\"}");
			});
	}

	void DiaChatPlugin::OnUnload()
	{
		DIA_LOG_INFO("Chat", "DiaChatPlugin: OnUnload");

		// Wait for any in-flight send to finish before tearing down.
		if (mSendThread.joinable())
			mSendThread.join();

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
		mActionQueue = nullptr;
	}

	void DiaChatPlugin::OnUpdate(float deltaTime)
	{
		if (mBridge != nullptr)
			mBridge->DoUpdate(deltaTime);
	}

	void DiaChatPlugin::DispatchSendMessage(const std::string& text,
	                                         const std::string& contextMode)
	{
		if (mSendInFlight.load())
		{
			DIA_LOG_WARNING("Chat", "DiaChatPlugin: ignoring send_message — previous send still in flight");
			return;
		}

		if (mSendThread.joinable())
			mSendThread.join();

		mSendInFlight.store(true);

		// Capture by value — the lambda must not reference plugin members that
		// could be destroyed before the thread finishes.
		std::string textCopy  = text;
		std::string modeCopy  = contextMode;
		std::atomic<bool>* inFlight = &mSendInFlight;

		mSendThread = std::thread([textCopy, modeCopy, inFlight]()
		{
			if (!Dia::Python::IsInitialized())
			{
				inFlight->store(false);
				return;
			}

			// Serialize text as a JSON string so arbitrary user input (quotes, newlines)
			// is safely embedded in the Python call.
			Json::Value textVal = textCopy;
			std::string textJson = SerializeJson(textVal);  // e.g. "\"hello\""

			char cmd[4096];
			snprintf(cmd, sizeof(cmd),
				"import dia_chat, json; "
				"dia_chat.send_message(json.loads(%s), context_mode='%s')",
				textJson.c_str(), modeCopy.c_str());

			// ExecuteStringOnThread acquires the GIL before calling into Python.
			Dia::Python::ExecuteStringOnThread(cmd);
			inFlight->store(false);
		});
	}
}

using namespace CluicheEditor;
REGISTER_EDITOR_PLUGIN(DiaChatPlugin, "DiaChatPlugin")
