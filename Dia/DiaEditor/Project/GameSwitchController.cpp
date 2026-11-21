#include "DiaEditor/Project/GameSwitchController.h"
#include "DiaEditor/EditorAPI/EditorActionRegistry.h"
#include "DiaEditor/EditorAPI/EditorActionDescriptor.h"
#include "DiaEditor/MVC/IEditorContext.h"
#include "DiaEditor/Plugin/IPluginLoader.h"
#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/CRC/StringCRC.h>

#include <string.h>

namespace Dia
{
	namespace Editor
	{
		static const Dia::Core::StringCRC kReqGetState("game_switch.get_state");
		static const Dia::Core::StringCRC kReqRespond("game_switch.respond");
		static const char* const kTopicChanged = "game_switch_changed";
		static const char* const kOwnerName = "GameSwitchController";

		namespace
		{
			// File stem of a path: last segment without extension.
			void ExtractStem(const char* path, char* out, unsigned int outSize)
			{
				out[0] = '\0';
				if (path == nullptr)
					return;
				const char* start = path;
				for (const char* c = path; *c; ++c)
					if (*c == '/' || *c == '\\') start = c + 1;
				unsigned int i = 0;
				while (start[i] && start[i] != '.' && i < outSize - 1)
				{
					out[i] = start[i];
					++i;
				}
				out[i] = '\0';
			}

			const char* PromptName(SwitchPrompt p)
			{
				switch (p)
				{
				case SwitchPrompt::kConfirmSwitch: return "confirm_switch";
				case SwitchPrompt::kUnsaved:       return "unsaved";
				case SwitchPrompt::kSaveFailed:    return "save_failed";
				default:                           return "none";
				}
			}

			const char* OutcomeName(SwitchOutcome o)
			{
				switch (o)
				{
				case SwitchOutcome::kSwitched:      return "switched";
				case SwitchOutcome::kDeclined:      return "declined";
				case SwitchOutcome::kCancelled:     return "cancelled";
				case SwitchOutcome::kLoadFailed:    return "load_failed";
				case SwitchOutcome::kAlreadyLoaded: return "already_loaded";
				case SwitchOutcome::kRejected:      return "rejected";
				default:                            return "none";
				}
			}
		}

		const Dia::Core::StringCRC GameSwitchController::kUniqueId("GameSwitchController");

		GameSwitchController::GameSwitchController()
			: mGate(*this)
		{
		}

		void GameSwitchController::Initialize(WebUIBridge* bridge, IEditorContext* context, IPluginLoader* loader,
		                                      EditorActionRegistry* api)
		{
			mBridge  = bridge;
			mContext = context;
			mLoader  = loader;
			mApi     = api;

			if (mBridge != nullptr)
			{
				mBridge->RegisterRequestHandler(kReqGetState, [this](const Json::Value& d) { return HandleGetState(d); });
				mBridge->RegisterRequestHandler(kReqRespond,  [this](const Json::Value& d) { return HandleRespond(d); });
			}

			if (mApi != nullptr)
			{
				EditorActionDescriptor getState;
				getState.name           = kReqGetState;
				getState.description    = "Return the pending game-switch prompt state: { pending, prompt, targetPath, targetName, currentName, isClose, dirty[], lastOutcome }.";
				getState.category       = "project";
				getState.owner          = kOwnerName;
				getState.dispatchThread = DispatchThread::kMainThread;
				getState.handler        = [this](const Json::Value& d) { return HandleGetState(d); };
				mApi->RegisterAction(getState);

				EditorActionDescriptor respond;
				respond.name           = kReqRespond;
				respond.description    = "Answer a pending game-switch prompt. Required param: choice ('cancel'|'confirm'|'discard'|'save'). Returns { ok, outcome, state }.";
				respond.category       = "project";
				respond.owner          = kOwnerName;
				respond.dispatchThread = DispatchThread::kMainThread;
				respond.handler        = [this](const Json::Value& d) { return HandleRespond(d); };
				mApi->RegisterAction(respond);
			}
		}

		void GameSwitchController::Shutdown()
		{
			if (mApi != nullptr)
			{
				mApi->DeregisterActionsForOwner(Dia::Core::StringCRC(kOwnerName));
				mApi = nullptr;
			}
			if (mBridge != nullptr)
			{
				mBridge->UnregisterRequestHandler(kReqGetState);
				mBridge->UnregisterRequestHandler(kReqRespond);
				mBridge = nullptr;
			}
			mContext = nullptr;
			mLoader  = nullptr;
		}

		SwitchOutcome GameSwitchController::RequestLoad(const char* path, SwitchOrigin origin)
		{
			return mGate.RequestLoad(path, origin);
		}

		SwitchOutcome GameSwitchController::RequestClose(SwitchOrigin origin)
		{
			return mGate.RequestClose(origin);
		}

		void GameSwitchController::OutcomeToResult(SwitchOutcome outcome, Json::Value& result)
		{
			switch (outcome)
			{
			case SwitchOutcome::kNone:
				result["ok"]      = true;
				result["pending"] = true;
				break;
			case SwitchOutcome::kSwitched:
			case SwitchOutcome::kAlreadyLoaded:
				result["ok"] = true;
				break;
			case SwitchOutcome::kLoadFailed:
				result["ok"]    = false;
				result["error"] = "failed to load project";
				break;
			case SwitchOutcome::kDeclined:
			case SwitchOutcome::kCancelled:
				result["ok"]        = false;
				result["cancelled"] = true;
				break;
			case SwitchOutcome::kRejected:
				result["ok"]    = false;
				result["error"] = "another game switch is pending";
				break;
			}
		}

		void GameSwitchController::BuildStateJson(Json::Value& out) const
		{
			char stem[64];
			out = Json::Value(Json::objectValue);
			out["pending"]    = mGate.IsPending();
			out["prompt"]     = PromptName(mGate.GetPrompt());
			out["targetPath"] = mGate.GetTargetPath();
			ExtractStem(mGate.GetTargetPath(), stem, sizeof(stem));
			out["targetName"] = stem;
			ExtractStem(GetLoadedGamePath(), stem, sizeof(stem));
			out["currentName"] = stem;
			out["isClose"]    = mGate.IsClose();

			Json::Value dirty(Json::arrayValue);
			for (unsigned int i = 0; i < mGate.GetDirtyCount(); ++i)
			{
				const SwitchDirtyEntry& e = mGate.GetDirty(i);
				Json::Value item;
				item["typeId"]     = e.typeId.AsChar();
				item["name"]       = e.name;
				item["saved"]      = e.saved;
				item["saveFailed"] = e.saveFailed;
				dirty.append(item);
			}
			out["dirty"]       = dirty;
			out["lastOutcome"] = OutcomeName(mGate.GetLastOutcome());
		}

		const char* GameSwitchController::GetLoadedGamePath() const
		{
			if (mContext == nullptr)
				return "";
			return mContext->GetDiagameProject().diagamePath;
		}

		unsigned int GameSwitchController::CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max)
		{
			return (mLoader != nullptr) ? mLoader->CollectDirtyGameScoped(out, max) : 0;
		}

		bool GameSwitchController::SavePlugin(const Dia::Core::StringCRC& typeId)
		{
			return (mLoader != nullptr) ? mLoader->SaveLoadedPlugin(typeId) : false;
		}

		bool GameSwitchController::LoadGame(const char* path)
		{
			return mContext != nullptr && mContext->LoadDiagameProject(path);
		}

		void GameSwitchController::CloseGame()
		{
			if (mContext != nullptr)
				mContext->ClearDiagameProject();
		}

		void GameSwitchController::OnGateStateChanged()
		{
			if (mBridge == nullptr)
				return;
			Json::Value state;
			BuildStateJson(state);
			mBridge->NotifyUIDataChanged(kTopicChanged, state);
		}

		Json::Value GameSwitchController::HandleGetState(const Json::Value& /*data*/)
		{
			Json::Value state;
			BuildStateJson(state);
			return state;
		}

		Json::Value GameSwitchController::HandleRespond(const Json::Value& data)
		{
			Json::Value result;
			if (!data.isObject() || !data.isMember("choice") || !data["choice"].isString())
			{
				result["ok"]    = false;
				result["error"] = "missing choice";
				return result;
			}

			const char* c = data["choice"].asCString();
			SwitchChoice choice;
			if (strcmp(c, "cancel") == 0)       choice = SwitchChoice::kCancel;
			else if (strcmp(c, "confirm") == 0) choice = SwitchChoice::kConfirm;
			else if (strcmp(c, "discard") == 0) choice = SwitchChoice::kDiscard;
			else if (strcmp(c, "save") == 0)    choice = SwitchChoice::kSave;
			else
			{
				result["ok"]    = false;
				result["error"] = "unknown choice";
				return result;
			}

			DIA_LOG_INFO("Editor", "GameSwitchController: respond '%s'", c);
			const SwitchOutcome outcome = mGate.Respond(choice);
			result["ok"]      = (outcome != SwitchOutcome::kRejected);
			result["outcome"] = OutcomeName(outcome);
			BuildStateJson(result["state"]);
			return result;
		}
	}
}
