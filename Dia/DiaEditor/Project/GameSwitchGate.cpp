#include "DiaEditor/Project/GameSwitchGate.h"
#include "DiaEditor/Project/GamePath.h"

#include <DiaObservation/Log/DiaLog.h>

#include <string.h>

namespace Dia
{
	namespace Editor
	{
		GameSwitchGate::GameSwitchGate(IGameSwitchHost& host)
			: mHost(host)
		{
		}

		void GameSwitchGate::NormalizeGamePath(const char* in, char* out, unsigned int outSize)
		{
			Dia::Editor::NormalizeGamePath(in, out, outSize);
		}

		bool GameSwitchGate::IsSameGame(const char* a, const char* b)
		{
			return IsSameGamePath(a, b);
		}

		void GameSwitchGate::SetPrompt(SwitchPrompt prompt)
		{
			mPrompt = prompt;
			mHost.OnGateStateChanged();
		}

		SwitchOutcome GameSwitchGate::Finish(SwitchOutcome outcome)
		{
			mPrompt = SwitchPrompt::kNone;
			mDirtyCount = 0;
			mLastOutcome = outcome;
			DIA_LOG_INFO("Editor", "GameSwitchGate: outcome=%d target='%s' close=%d", static_cast<int>(outcome), mTargetPath, mIsClose ? 1 : 0);
			mHost.OnGateStateChanged();
			return outcome;
		}

		SwitchOutcome GameSwitchGate::Execute()
		{
			if (mIsClose)
			{
				mHost.CloseGame();
				return Finish(SwitchOutcome::kSwitched);
			}
			if (!mHost.LoadGame(mTargetPath))
			{
				return Finish(SwitchOutcome::kLoadFailed);
			}
			return Finish(SwitchOutcome::kSwitched);
		}

		SwitchOutcome GameSwitchGate::BeginDirtyCheck()
		{
			mDirtyCount = mHost.CollectDirtyGameScoped(mDirty, kMaxDirty);
			if (mDirtyCount > kMaxDirty)
			{
				mDirtyCount = kMaxDirty;
			}
			if (mDirtyCount == 0)
			{
				return Execute();
			}
			for (unsigned int i = 0; i < mDirtyCount; ++i)
			{
				mDirty[i].saved = false;
				mDirty[i].saveFailed = false;
			}
			DIA_LOG_INFO("Editor", "GameSwitchGate: unsaved prompt, %u dirty plugin(s)", mDirtyCount);
			SetPrompt(SwitchPrompt::kUnsaved);
			return SwitchOutcome::kNone;
		}

		SwitchOutcome GameSwitchGate::TrySaveAll()
		{
			bool anyFailed = false;
			for (unsigned int i = 0; i < mDirtyCount; ++i)
			{
				if (mDirty[i].saved)
				{
					continue;
				}
				if (mHost.SavePlugin(mDirty[i].typeId))
				{
					mDirty[i].saved = true;
					mDirty[i].saveFailed = false;
				}
				else
				{
					mDirty[i].saveFailed = true;
					anyFailed = true;
				}
			}
			if (!anyFailed)
			{
				return Execute();
			}
			DIA_LOG_INFO("Editor", "GameSwitchGate: save failed for one or more plugins");
			SetPrompt(SwitchPrompt::kSaveFailed);
			return SwitchOutcome::kNone;
		}

		SwitchOutcome GameSwitchGate::RequestLoad(const char* path, SwitchOrigin origin)
		{
			if (IsPending())
			{
				DIA_LOG_INFO("Editor", "GameSwitchGate: load request rejected, a prompt is pending");
				return SwitchOutcome::kRejected;
			}
			if (path == nullptr || path[0] == '\0')
			{
				DIA_LOG_INFO("Editor", "GameSwitchGate: load request rejected, empty path");
				return SwitchOutcome::kRejected;
			}

			strncpy_s(mTargetPath, kMaxPath, path, _TRUNCATE);
			mIsClose = false;
			mDirtyCount = 0;

			const char* loaded = mHost.GetLoadedGamePath();
			const bool hasGame = loaded != nullptr && loaded[0] != '\0';
			DIA_LOG_INFO("Editor", "GameSwitchGate: load request '%s' origin=%s loaded='%s'", path,
				origin == SwitchOrigin::kLiveConnection ? "live" : "user", hasGame ? loaded : "");

			if (!hasGame)
			{
				return Execute();
			}
			if (IsSameGamePath(loaded, path))
			{
				if (origin == SwitchOrigin::kLiveConnection)
				{
					return Finish(SwitchOutcome::kAlreadyLoaded);
				}
				return Execute();
			}
			if (origin == SwitchOrigin::kLiveConnection)
			{
				DIA_LOG_INFO("Editor", "GameSwitchGate: confirm-switch prompt for '%s'", path);
				SetPrompt(SwitchPrompt::kConfirmSwitch);
				return SwitchOutcome::kNone;
			}
			return BeginDirtyCheck();
		}

		SwitchOutcome GameSwitchGate::RequestClose(SwitchOrigin origin)
		{
			(void)origin;
			if (IsPending())
			{
				DIA_LOG_INFO("Editor", "GameSwitchGate: close request rejected, a prompt is pending");
				return SwitchOutcome::kRejected;
			}
			const char* loaded = mHost.GetLoadedGamePath();
			if (loaded == nullptr || loaded[0] == '\0')
			{
				DIA_LOG_INFO("Editor", "GameSwitchGate: close request rejected, no game loaded");
				return SwitchOutcome::kRejected;
			}
			mTargetPath[0] = '\0';
			mIsClose = true;
			mDirtyCount = 0;
			DIA_LOG_INFO("Editor", "GameSwitchGate: close request for '%s'", loaded);
			return BeginDirtyCheck();
		}

		SwitchOutcome GameSwitchGate::Respond(SwitchChoice choice)
		{
			switch (mPrompt)
			{
			case SwitchPrompt::kConfirmSwitch:
				if (choice == SwitchChoice::kCancel)
				{
					DIA_LOG_INFO("Editor", "GameSwitchGate: switch declined");
					return Finish(SwitchOutcome::kDeclined);
				}
				if (choice == SwitchChoice::kConfirm)
				{
					DIA_LOG_INFO("Editor", "GameSwitchGate: switch confirmed");
					return BeginDirtyCheck();
				}
				break;
			case SwitchPrompt::kUnsaved:
			case SwitchPrompt::kSaveFailed:
				if (choice == SwitchChoice::kCancel)
				{
					DIA_LOG_INFO("Editor", "GameSwitchGate: cancelled at unsaved prompt");
					return Finish(SwitchOutcome::kCancelled);
				}
				if (choice == SwitchChoice::kDiscard)
				{
					DIA_LOG_INFO("Editor", "GameSwitchGate: discarding unsaved changes");
					return Execute();
				}
				if (choice == SwitchChoice::kSave)
				{
					DIA_LOG_INFO("Editor", "GameSwitchGate: saving dirty plugins");
					return TrySaveAll();
				}
				break;
			case SwitchPrompt::kNone:
				break;
			}
			DIA_LOG_INFO("Editor", "GameSwitchGate: response %d rejected at prompt %d", static_cast<int>(choice), static_cast<int>(mPrompt));
			return SwitchOutcome::kRejected;
		}
	}
}
