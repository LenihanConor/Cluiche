#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		enum class SwitchOrigin { kUser, kLiveConnection };
		enum class SwitchPrompt { kNone, kConfirmSwitch, kUnsaved, kSaveFailed };
		// kConfirm = "Switch" at kConfirmSwitch. kSave also means "Retry save" at kSaveFailed.
		enum class SwitchChoice { kCancel, kConfirm, kDiscard, kSave };
		enum class SwitchOutcome { kNone, kSwitched, kDeclined, kCancelled, kLoadFailed, kAlreadyLoaded, kRejected };

		struct SwitchDirtyEntry
		{
			Dia::Core::StringCRC typeId;
			char name[64];
			bool saved;
			bool saveFailed;
		};

		// Implemented by the editor (faked in tests). The gate knows nothing about UI or bridges.
		class IGameSwitchHost
		{
		public:
			virtual ~IGameSwitchHost() = default;
			virtual const char* GetLoadedGamePath() const = 0;   // "" or null = none loaded
			virtual unsigned int CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max) = 0;
			virtual bool SavePlugin(const Dia::Core::StringCRC& typeId) = 0;
			virtual bool LoadGame(const char* path) = 0;
			virtual void CloseGame() = 0;
			// Called exactly once per gate state change (prompt shown, prompt changed, completion).
			virtual void OnGateStateChanged() = 0;
		};

		// Decision logic in front of IEditorContext::LoadDiagameProject / ClearDiagameProject.
		// One request at a time; prompts are answered through Respond().
		class GameSwitchGate
		{
		public:
			static const unsigned int kMaxDirty = 16;
			static const unsigned int kMaxPath = 260;

			explicit GameSwitchGate(IGameSwitchHost& host);

			// Return kNone while a prompt is pending (answer via Respond).
			SwitchOutcome RequestLoad(const char* path, SwitchOrigin origin);
			SwitchOutcome RequestClose(SwitchOrigin origin);
			SwitchOutcome Respond(SwitchChoice choice);

			bool IsPending() const { return mPrompt != SwitchPrompt::kNone; }
			SwitchPrompt GetPrompt() const { return mPrompt; }
			const char* GetTargetPath() const { return mTargetPath; }
			bool IsClose() const { return mIsClose; }
			unsigned int GetDirtyCount() const { return mDirtyCount; }
			const SwitchDirtyEntry& GetDirty(unsigned int i) const { return mDirty[i]; }
			SwitchOutcome GetLastOutcome() const { return mLastOutcome; }

			static void NormalizeGamePath(const char* in, char* out, unsigned int outSize);
			static bool IsSameGame(const char* a, const char* b);

		private:
			SwitchOutcome BeginDirtyCheck();   // after any confirm: dirty prompt or execute
			SwitchOutcome Execute();           // performs load/close and completes
			SwitchOutcome Finish(SwitchOutcome outcome);
			void SetPrompt(SwitchPrompt prompt);
			SwitchOutcome TrySaveAll();

			IGameSwitchHost& mHost;
			SwitchPrompt mPrompt = SwitchPrompt::kNone;
			char mTargetPath[kMaxPath] = {};
			bool mIsClose = false;
			SwitchDirtyEntry mDirty[kMaxDirty] = {};
			unsigned int mDirtyCount = 0;
			SwitchOutcome mLastOutcome = SwitchOutcome::kNone;
		};
	}
}
