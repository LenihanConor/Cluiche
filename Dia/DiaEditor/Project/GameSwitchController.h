#pragma once

#include <DiaEditor/Project/GameSwitchGate.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class IEditorContext;
		class IPluginLoader;
		class WebUIBridge;
		class EditorActionRegistry;

		// Bridge adapter over GameSwitchGate. Implements IGameSwitchHost against the editor context
		// and plugin loader, registers game_switch.* handlers, and pushes "game_switch_changed".
		class GameSwitchController : public IGameSwitchHost
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;  // StringCRC("GameSwitchController")

			GameSwitchController();

			void Initialize(WebUIBridge* bridge, IEditorContext* context, IPluginLoader* loader,
			                EditorActionRegistry* api = nullptr);
			void Shutdown();

			// Entry points for other controllers.
			SwitchOutcome RequestLoad(const char* path, SwitchOrigin origin);
			SwitchOutcome RequestClose(SwitchOrigin origin);

			// Fills the result JSON the project.* handlers return for an outcome.
			static void OutcomeToResult(SwitchOutcome outcome, Json::Value& result);

			void BuildStateJson(Json::Value& out) const;
			const GameSwitchGate& GetGate() const { return mGate; }

			// IGameSwitchHost
			const char* GetLoadedGamePath() const override;
			unsigned int CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max) override;
			bool SavePlugin(const Dia::Core::StringCRC& typeId) override;
			bool LoadGame(const char* path) override;
			void CloseGame() override;
			void OnGateStateChanged() override;

		private:
			Json::Value HandleGetState(const Json::Value& data);
			Json::Value HandleRespond(const Json::Value& data);

			WebUIBridge*          mBridge  = nullptr;
			IEditorContext*       mContext = nullptr;
			IPluginLoader*        mLoader  = nullptr;
			EditorActionRegistry* mApi     = nullptr;
			GameSwitchGate        mGate;
		};
	}
}
