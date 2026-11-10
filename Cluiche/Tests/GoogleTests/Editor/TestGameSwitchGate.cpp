#include <gtest/gtest.h>
#include <DiaEditor/Project/GameSwitchGate.h>
#include <DiaEditor/Project/GamePath.h>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>

using namespace Dia::Editor;
using Dia::Core::StringCRC;

namespace
{
	// Fake host. OnGateStateChanged is counted: the gate calls it exactly once per state change
	// (a request that needs a prompt, each response that changes state, and each completion).
	class FakeHost : public IGameSwitchHost
	{
	public:
		std::string loaded;
		std::vector<SwitchDirtyEntry> dirty;
		std::vector<StringCRC> saveFail; // typeIds whose save fails (until cleared)
		bool loadResult = true;
		int loadCalls = 0, closeCalls = 0, changed = 0;
		std::vector<StringCRC> saved;
		std::string lastLoad;

		const char* GetLoadedGamePath() const override { return loaded.c_str(); }
		unsigned int CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max) override
		{
			unsigned int n = 0;
			for (auto& d : dirty) { if (n < max) out[n++] = d; }
			return n;
		}
		bool SavePlugin(const StringCRC& id) override
		{
			saved.push_back(id);
			for (auto& f : saveFail) if (f == id) return false;
			return true;
		}
		bool LoadGame(const char* p) override { ++loadCalls; lastLoad = p; if (loadResult) loaded = p; return loadResult; }
		void CloseGame() override { ++closeCalls; loaded.clear(); }
		void OnGateStateChanged() override { ++changed; }

		void AddDirty(const char* name)
		{
			SwitchDirtyEntry e{};
			e.typeId = StringCRC(name);
			strncpy_s(e.name, name, _TRUNCATE);
			dirty.push_back(e);
		}
	};
}

// --- paths ---

TEST(GamePath, DotSegmentsRemoved) { EXPECT_TRUE(IsSameGamePath("a/./b.diagame", "a/b.diagame")); }
TEST(GamePath, DotDotRemoved) { EXPECT_TRUE(IsSameGamePath("a/c/../b.diagame", "a/b.diagame")); }
TEST(GamePath, CaseAndSeparatorInsensitive) { EXPECT_TRUE(IsSameGamePath("A\\B.diagame", "a/b.diagame")); }
TEST(GamePath, RelativeEqualsAbsolute)
{
	std::string abs = std::filesystem::absolute("a/b.diagame").string();
	EXPECT_TRUE(IsSameGamePath("a/b.diagame", abs.c_str()));
}
TEST(GamePath, DifferentDirsDiffer) { EXPECT_FALSE(IsSameGamePath("a/b.diagame", "c/b.diagame")); }
TEST(GamePath, EmptyAndNullNeverSame)
{
	EXPECT_FALSE(IsSameGamePath("", ""));
	EXPECT_FALSE(IsSameGamePath(nullptr, nullptr));
	EXPECT_FALSE(IsSameGamePath("a.diagame", ""));
	EXPECT_FALSE(IsSameGamePath(nullptr, "a.diagame"));
}
TEST(GamePath, NormalizeIsLowercaseForwardSlash)
{
	char out[260];
	NormalizeGamePath("A\\B.DIAGAME", out, sizeof(out));
	EXPECT_EQ(strchr(out, '\\'), nullptr);
	EXPECT_EQ(strpbrk(out, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"), nullptr);
}
TEST(GamePath, GateStaticsMatchFreeFunctions)
{
	EXPECT_TRUE(GameSwitchGate::IsSameGame("A/./b.diagame", "a/b.diagame"));
}

// --- gate ---

TEST(GameSwitchGate, NoGameLoaded_LoadsImmediately)
{
	FakeHost h; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("a.diagame", SwitchOrigin::kLiveConnection), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.loadCalls, 1);
	EXPECT_FALSE(g.IsPending());
	EXPECT_EQ(g.GetLastOutcome(), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.changed, 1);
}

TEST(GameSwitchGate, NoGameLoaded_LoadFailure)
{
	FakeHost h; h.loadResult = false; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("a.diagame", SwitchOrigin::kUser), SwitchOutcome::kLoadFailed);
	EXPECT_FALSE(g.IsPending());
	EXPECT_EQ(g.GetLastOutcome(), SwitchOutcome::kLoadFailed);
}

TEST(GameSwitchGate, SameGameLive_AlreadyLoaded)
{
	FakeHost h; h.loaded = "a/b.diagame"; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("A/./B.diagame", SwitchOrigin::kLiveConnection), SwitchOutcome::kAlreadyLoaded);
	EXPECT_EQ(h.loadCalls, 0);
	EXPECT_FALSE(g.IsPending());
}

TEST(GameSwitchGate, SameGameUser_ReloadsWithoutPrompt)
{
	FakeHost h; h.loaded = "a/b.diagame"; h.AddDirty("P"); GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("a/./b.diagame", SwitchOrigin::kUser), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.loadCalls, 1);
	EXPECT_EQ(h.closeCalls, 0);
}

TEST(GameSwitchGate, LiveDifferentGame_ConfirmThenSwitch)
{
	FakeHost h; h.loaded = "a.diagame"; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("b.diagame", SwitchOrigin::kLiveConnection), SwitchOutcome::kNone);
	EXPECT_TRUE(g.IsPending());
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kConfirmSwitch);
	EXPECT_STREQ(g.GetTargetPath(), "b.diagame");
	EXPECT_FALSE(g.IsClose());
	EXPECT_EQ(h.loadCalls, 0);
	EXPECT_EQ(g.Respond(SwitchChoice::kConfirm), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.loadCalls, 1);
	EXPECT_FALSE(g.IsPending());
}

TEST(GameSwitchGate, LiveDifferentGame_DeclineKeepsGame)
{
	FakeHost h; h.loaded = "a.diagame"; GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kLiveConnection);
	EXPECT_EQ(g.Respond(SwitchChoice::kCancel), SwitchOutcome::kDeclined);
	EXPECT_EQ(h.loadCalls, 0); EXPECT_EQ(h.closeCalls, 0);
	EXPECT_EQ(h.loaded, "a.diagame");
	EXPECT_FALSE(g.IsPending());
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kNone);
	EXPECT_EQ(g.GetLastOutcome(), SwitchOutcome::kDeclined);
	EXPECT_EQ(h.changed, 2);
}

TEST(GameSwitchGate, UserDifferentGame_NoConfirmStep)
{
	FakeHost h; h.loaded = "a.diagame"; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("b.diagame", SwitchOrigin::kUser), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.loadCalls, 1);
}

TEST(GameSwitchGate, DirtyPrompt_CancelKeepsGame)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.AddDirty("P2"); GameSwitchGate g(h);
	EXPECT_EQ(g.RequestLoad("b.diagame", SwitchOrigin::kUser), SwitchOutcome::kNone);
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kUnsaved);
	ASSERT_EQ(g.GetDirtyCount(), 2u);
	EXPECT_STREQ(g.GetDirty(1).name, "P2");
	EXPECT_EQ(g.Respond(SwitchChoice::kCancel), SwitchOutcome::kCancelled);
	EXPECT_EQ(h.loadCalls, 0); EXPECT_TRUE(h.saved.empty());
	EXPECT_FALSE(g.IsPending());
}

TEST(GameSwitchGate, DirtyPrompt_DiscardSwitchesWithoutSaving)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kSwitched);
	EXPECT_TRUE(h.saved.empty());
	EXPECT_EQ(h.loadCalls, 1);
}

TEST(GameSwitchGate, DirtyPrompt_SaveAllThenSwitch)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.AddDirty("P2"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	EXPECT_EQ(g.Respond(SwitchChoice::kSave), SwitchOutcome::kSwitched);
	ASSERT_EQ(h.saved.size(), 2u);
	EXPECT_TRUE(h.saved[0] == StringCRC("P1"));
	EXPECT_TRUE(h.saved[1] == StringCRC("P2"));
	EXPECT_EQ(h.loadCalls, 1);
}

TEST(GameSwitchGate, SaveFailure_PromptsAndRetryOnlyFailed)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.AddDirty("P2");
	h.saveFail.push_back(StringCRC("P2")); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	EXPECT_EQ(g.Respond(SwitchChoice::kSave), SwitchOutcome::kNone);
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kSaveFailed);
	EXPECT_TRUE(g.GetDirty(0).saved);   EXPECT_FALSE(g.GetDirty(0).saveFailed);
	EXPECT_FALSE(g.GetDirty(1).saved);  EXPECT_TRUE(g.GetDirty(1).saveFailed);
	EXPECT_EQ(h.loadCalls, 0);
	h.saveFail.clear(); h.saved.clear();
	EXPECT_EQ(g.Respond(SwitchChoice::kSave), SwitchOutcome::kSwitched);
	ASSERT_EQ(h.saved.size(), 1u);
	EXPECT_TRUE(h.saved[0] == StringCRC("P2"));
	EXPECT_EQ(h.loadCalls, 1);
}

TEST(GameSwitchGate, SaveFailure_DiscardProceeds)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.saveFail.push_back(StringCRC("P1")); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	g.Respond(SwitchChoice::kSave);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.loadCalls, 1);
}

TEST(GameSwitchGate, SaveFailure_CancelKeepsGame)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.saveFail.push_back(StringCRC("P1")); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	g.Respond(SwitchChoice::kSave);
	EXPECT_EQ(g.Respond(SwitchChoice::kCancel), SwitchOutcome::kCancelled);
	EXPECT_EQ(h.loadCalls, 0); EXPECT_EQ(h.closeCalls, 0);
}

TEST(GameSwitchGate, LiveConfirmThenDirtyPrompt)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kLiveConnection);
	EXPECT_EQ(g.Respond(SwitchChoice::kConfirm), SwitchOutcome::kNone);
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kUnsaved);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kSwitched);
}

TEST(GameSwitchGate, LoadFailureAfterPrompt_NotSwitched)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); h.loadResult = false; GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kLoadFailed);
	EXPECT_FALSE(g.IsPending());
	EXPECT_EQ(g.GetLastOutcome(), SwitchOutcome::kLoadFailed);
}

TEST(GameSwitchGate, CloseClean_ClosesImmediately)
{
	FakeHost h; h.loaded = "a.diagame"; GameSwitchGate g(h);
	EXPECT_EQ(g.RequestClose(SwitchOrigin::kUser), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.closeCalls, 1);
}

TEST(GameSwitchGate, CloseDirty_PromptsThenDiscardCloses)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	EXPECT_EQ(g.RequestClose(SwitchOrigin::kUser), SwitchOutcome::kNone);
	EXPECT_TRUE(g.IsClose());
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kUnsaved);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kSwitched);
	EXPECT_EQ(h.closeCalls, 1); EXPECT_EQ(h.loadCalls, 0);
}

TEST(GameSwitchGate, CloseDirty_CancelKeepsGame)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestClose(SwitchOrigin::kUser);
	EXPECT_EQ(g.Respond(SwitchChoice::kCancel), SwitchOutcome::kCancelled);
	EXPECT_EQ(h.closeCalls, 0);
}

TEST(GameSwitchGate, RequestWhilePending_Rejected)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kUser);
	int before = h.changed;
	EXPECT_EQ(g.RequestLoad("c.diagame", SwitchOrigin::kUser), SwitchOutcome::kRejected);
	EXPECT_EQ(g.RequestClose(SwitchOrigin::kUser), SwitchOutcome::kRejected);
	EXPECT_STREQ(g.GetTargetPath(), "b.diagame");
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kUnsaved);
	EXPECT_EQ(h.changed, before);
}

TEST(GameSwitchGate, RespondWithNothingPending_Rejected)
{
	FakeHost h; GameSwitchGate g(h);
	EXPECT_EQ(g.Respond(SwitchChoice::kSave), SwitchOutcome::kRejected);
	EXPECT_EQ(h.changed, 0);
}

TEST(GameSwitchGate, InvalidChoicesRejected)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kLiveConnection);
	EXPECT_EQ(g.Respond(SwitchChoice::kDiscard), SwitchOutcome::kRejected);
	EXPECT_EQ(g.Respond(SwitchChoice::kSave), SwitchOutcome::kRejected);
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kConfirmSwitch);
	g.Respond(SwitchChoice::kConfirm);
	EXPECT_EQ(g.Respond(SwitchChoice::kConfirm), SwitchOutcome::kRejected);
	EXPECT_EQ(g.GetPrompt(), SwitchPrompt::kUnsaved);
}

TEST(GameSwitchGate, StateChangedOncePerChange)
{
	FakeHost h; h.loaded = "a.diagame"; h.AddDirty("P1"); GameSwitchGate g(h);
	g.RequestLoad("b.diagame", SwitchOrigin::kLiveConnection);
	EXPECT_EQ(h.changed, 1);
	g.Respond(SwitchChoice::kConfirm);
	EXPECT_EQ(h.changed, 2);
	g.Respond(SwitchChoice::kDiscard);
	EXPECT_EQ(h.changed, 3);
}
