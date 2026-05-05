#include <gtest/gtest.h>

#include <DiaAssetRuntimeEditor/Panels/AssetStateRow.h>
#include <DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h>
#include <DiaAssetRuntimeEditor/Panels/TransitionLogEntry.h>
#include <DiaAssetRuntimeEditor/Panels/StateTransitionLogPanel.h>
#include <DiaAssetRuntimeEditor/Panels/RefCountInspectorPanel.h>
#include <DiaAssetRuntimeEditor/Panels/StageAssetTreePanel.h>
#include <DiaAssetRuntimeEditor/SharedPluginState.h>
#include <DiaAssetRuntimeEditor/SessionContext.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>

using namespace Dia::AssetRuntime::Editor;
using namespace Dia::Core;

// ===========================================================================
// AssetStateRow — Enum conversion tests
// ===========================================================================

TEST(AssetStateRow, StateEnumToString_AllValues)
{
	EXPECT_STREQ("Registered", AssetStateEnumToString(AssetStateEnum::kRegistered));
	EXPECT_STREQ("Staged", AssetStateEnumToString(AssetStateEnum::kStaged));
	EXPECT_STREQ("Loaded", AssetStateEnumToString(AssetStateEnum::kLoaded));
	EXPECT_STREQ("Unloading", AssetStateEnumToString(AssetStateEnum::kUnloading));
}

TEST(AssetStateRow, StringToStateEnum_ValidStrings)
{
	EXPECT_EQ(AssetStateEnum::kRegistered, StringToAssetStateEnum("Registered"));
	EXPECT_EQ(AssetStateEnum::kStaged, StringToAssetStateEnum("Staged"));
	EXPECT_EQ(AssetStateEnum::kLoaded, StringToAssetStateEnum("Loaded"));
	EXPECT_EQ(AssetStateEnum::kUnloading, StringToAssetStateEnum("Unloading"));
}

TEST(AssetStateRow, StringToStateEnum_InvalidString_DefaultsToRegistered)
{
	EXPECT_EQ(AssetStateEnum::kRegistered, StringToAssetStateEnum("Unknown"));
	EXPECT_EQ(AssetStateEnum::kRegistered, StringToAssetStateEnum(""));
	EXPECT_EQ(AssetStateEnum::kRegistered, StringToAssetStateEnum(nullptr));
}

TEST(AssetStateRow, ScopeEnumToString_AllValues)
{
	EXPECT_STREQ("Global", AssetScopeEnumToString(AssetScopeEnum::kGlobal));
	EXPECT_STREQ("Stage", AssetScopeEnumToString(AssetScopeEnum::kStage));
}

TEST(AssetStateRow, StringToScopeEnum_ValidStrings)
{
	EXPECT_EQ(AssetScopeEnum::kGlobal, StringToAssetScopeEnum("Global"));
	EXPECT_EQ(AssetScopeEnum::kGlobal, StringToAssetScopeEnum("global"));
	EXPECT_EQ(AssetScopeEnum::kStage, StringToAssetScopeEnum("Stage"));
	EXPECT_EQ(AssetScopeEnum::kStage, StringToAssetScopeEnum("stage"));
}

TEST(AssetStateRow, StringToScopeEnum_InvalidString_DefaultsToGlobal)
{
	EXPECT_EQ(AssetScopeEnum::kGlobal, StringToAssetScopeEnum("something"));
	EXPECT_EQ(AssetScopeEnum::kGlobal, StringToAssetScopeEnum(nullptr));
}

TEST(AssetStateRow, DefaultConstruction)
{
	AssetStateRow row;
	EXPECT_EQ(AssetStateEnum::kRegistered, row.mState);
	EXPECT_EQ(AssetScopeEnum::kGlobal, row.mScope);
	EXPECT_EQ(0u, row.mRefCount);
}

// ===========================================================================
// AssetStateTablePanel — JSON Parsing
// ===========================================================================

class AssetStateTablePanelParseTest : public ::testing::Test
{
protected:
	void SetUp() override { mRows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>(); }
	std::unique_ptr<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>> mRows;
};

TEST_F(AssetStateTablePanelParseTest, ParseEmptyResponse)
{
	Json::Value data;
	data["assets"] = Json::Value(Json::arrayValue);
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);
	EXPECT_EQ(0u, mRows->Size());
}

TEST_F(AssetStateTablePanelParseTest, ParseSingleAsset)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["id"] = "texture.player";
	asset["state"] = "Loaded";
	asset["scope"] = "Global";
	asset["refCount"] = 3;
	asset["deployPath"] = "C:/game/assets/texture_player.dds";
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(StringCRC("texture.player"), (*mRows)[0].mAssetId);
	EXPECT_EQ(AssetStateEnum::kLoaded, (*mRows)[0].mState);
	EXPECT_EQ(AssetScopeEnum::kGlobal, (*mRows)[0].mScope);
	EXPECT_EQ(3u, (*mRows)[0].mRefCount);
	EXPECT_STREQ("C:/game/assets/texture_player.dds", (*mRows)[0].mDeployPath.AsCStr());
}

TEST_F(AssetStateTablePanelParseTest, ParseMultipleAssets)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);

	const char* ids[] = {"tex.a", "tex.b", "mesh.c", "audio.d"};
	const char* states[] = {"Registered", "Staged", "Loaded", "Unloading"};

	for (int i = 0; i < 4; ++i)
	{
		Json::Value asset;
		asset["id"] = ids[i];
		asset["state"] = states[i];
		asset["scope"] = "Global";
		asset["refCount"] = i + 1;
		asset["deployPath"] = "";
		assets.append(asset);
	}
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(4u, mRows->Size());
	EXPECT_EQ(AssetStateEnum::kRegistered, (*mRows)[0].mState);
	EXPECT_EQ(AssetStateEnum::kStaged, (*mRows)[1].mState);
	EXPECT_EQ(AssetStateEnum::kLoaded, (*mRows)[2].mState);
	EXPECT_EQ(AssetStateEnum::kUnloading, (*mRows)[3].mState);
}

TEST_F(AssetStateTablePanelParseTest, ParseStageScopedAsset)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["id"] = "level.bg";
	asset["state"] = "Staged";
	asset["scope"] = "Stage";
	asset["refCount"] = 1;
	asset["deployPath"] = "/deploy/bg.png";
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(AssetScopeEnum::kStage, (*mRows)[0].mScope);
	EXPECT_EQ(1u, (*mRows)[0].mRefCount);
}

TEST_F(AssetStateTablePanelParseTest, ParseMissingFields_DefaultsUsed)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["id"] = "minimal.asset";
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(StringCRC("minimal.asset"), (*mRows)[0].mAssetId);
	EXPECT_EQ(AssetStateEnum::kRegistered, (*mRows)[0].mState);
	EXPECT_EQ(AssetScopeEnum::kGlobal, (*mRows)[0].mScope);
	EXPECT_EQ(0u, (*mRows)[0].mRefCount);
}

TEST_F(AssetStateTablePanelParseTest, ParseNoAssetsKey_NoRows)
{
	Json::Value data;
	data["something"] = "else";
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);
	EXPECT_EQ(0u, mRows->Size());
}

TEST_F(AssetStateTablePanelParseTest, ParseAssetsNotArray_NoRows)
{
	Json::Value data;
	data["assets"] = "not an array";
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);
	EXPECT_EQ(0u, mRows->Size());
}

// ===========================================================================
// AssetStateTablePanel — Poll timer and connection state
// ===========================================================================

TEST(AssetStateTablePanel, DefaultPollInterval)
{
	auto panel = std::make_unique<AssetStateTablePanel>();
	EXPECT_FLOAT_EQ(1.0f, panel->GetPollInterval());
}

TEST(AssetStateTablePanel, SetPollInterval_ClampMinimum)
{
	auto panel = std::make_unique<AssetStateTablePanel>();
	panel->SetPollInterval(0.01f);
	EXPECT_GE(panel->GetPollInterval(), 0.1f);
}

TEST(AssetStateTablePanel, SetPollInterval_NormalValue)
{
	auto panel = std::make_unique<AssetStateTablePanel>();
	panel->SetPollInterval(2.5f);
	EXPECT_FLOAT_EQ(2.5f, panel->GetPollInterval());
}

// ===========================================================================
// SharedPluginState
// ===========================================================================

TEST(SharedPluginState, DefaultConstruction)
{
	SharedPluginState state;
	EXPECT_EQ(StringCRC(), state.mSelectedAssetId);
	EXPECT_FALSE(state.mConnected);
	EXPECT_EQ(0u, state.mSnapshotVersion);
}

TEST(SharedPluginState, SelectionAndConnection)
{
	SharedPluginState state;
	state.mSelectedAssetId = StringCRC("test.asset");
	state.mConnected = true;
	state.mSnapshotVersion = 42;

	EXPECT_EQ(StringCRC("test.asset"), state.mSelectedAssetId);
	EXPECT_TRUE(state.mConnected);
	EXPECT_EQ(42u, state.mSnapshotVersion);
}

// ===========================================================================
// TransitionLogEntry
// ===========================================================================

TEST(TransitionLogEntry, DefaultConstruction)
{
	TransitionLogEntry entry;
	EXPECT_EQ(LogEntryType::kTransition, entry.mType);
	EXPECT_EQ(AssetStateEnum::kRegistered, entry.mOldState);
	EXPECT_EQ(AssetStateEnum::kRegistered, entry.mNewState);
	EXPECT_EQ(0ull, entry.mTimestamp);
}

TEST(TransitionLogEntry, MarkerTypes)
{
	TransitionLogEntry disconnect;
	disconnect.mType = LogEntryType::kMarkerDisconnect;
	EXPECT_EQ(LogEntryType::kMarkerDisconnect, disconnect.mType);

	TransitionLogEntry reconnect;
	reconnect.mType = LogEntryType::kMarkerReconnect;
	EXPECT_EQ(LogEntryType::kMarkerReconnect, reconnect.mType);
}

// ===========================================================================
// StateTransitionLogPanel — FIFO and max entries
// ===========================================================================

TEST(StateTransitionLogPanel, DefaultMaxEntries)
{
	StateTransitionLogPanel panel;
	EXPECT_EQ(1000u, panel.GetMaxEntries());
}

TEST(StateTransitionLogPanel, SetMaxEntries_ClampMin)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(1);
	EXPECT_GE(panel.GetMaxEntries(), 10u);
}

TEST(StateTransitionLogPanel, SetMaxEntries_ClampMax)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(100000);
	EXPECT_LE(panel.GetMaxEntries(), StateTransitionLogPanel::kMaxLogCapacity);
}

TEST(StateTransitionLogPanel, SetMaxEntries_NormalValue)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(500);
	EXPECT_EQ(500u, panel.GetMaxEntries());
}

TEST(StateTransitionLogPanel, PauseResume)
{
	StateTransitionLogPanel panel;
	EXPECT_FALSE(panel.IsPaused());
	panel.SetPaused(true);
	EXPECT_TRUE(panel.IsPaused());
	panel.SetPaused(false);
	EXPECT_FALSE(panel.IsPaused());
}

// ===========================================================================
// SessionContext
// ===========================================================================

TEST(SessionContext, DefaultValues)
{
	SessionContext ctx;
	EXPECT_FLOAT_EQ(1.0f, ctx.GetPollInterval());
	EXPECT_EQ(1000u, ctx.GetMaxLogEntries());
}

TEST(SessionContext, SetPollInterval_Normal)
{
	SessionContext ctx;
	ctx.SetPollInterval(2.0f);
	EXPECT_FLOAT_EQ(2.0f, ctx.GetPollInterval());
}

TEST(SessionContext, SetPollInterval_ClampMin)
{
	SessionContext ctx;
	ctx.SetPollInterval(0.01f);
	EXPECT_GE(ctx.GetPollInterval(), 0.1f);
}

TEST(SessionContext, SetMaxLogEntries_Normal)
{
	SessionContext ctx;
	ctx.SetMaxLogEntries(500);
	EXPECT_EQ(500u, ctx.GetMaxLogEntries());
}

TEST(SessionContext, SetMaxLogEntries_ClampMin)
{
	SessionContext ctx;
	ctx.SetMaxLogEntries(1);
	EXPECT_GE(ctx.GetMaxLogEntries(), 10u);
}

TEST(SessionContext, SetMaxLogEntries_ClampMax)
{
	SessionContext ctx;
	ctx.SetMaxLogEntries(999999);
	EXPECT_LE(ctx.GetMaxLogEntries(), 4096u);
}

TEST(SessionContext, SaveAndLoad_RoundTrip)
{
	const char* testDir = "Cluiche/out/test_session_context_runtime";

	{
		SessionContext ctx;
		ctx.SetPollInterval(3.5f);
		ctx.SetMaxLogEntries(750);
		ctx.Save(testDir);
	}

	{
		SessionContext ctx;
		ctx.Load(testDir);
		EXPECT_FLOAT_EQ(3.5f, ctx.GetPollInterval());
		EXPECT_EQ(750u, ctx.GetMaxLogEntries());
	}
}

TEST(SessionContext, Load_MissingFile_UsesDefaults)
{
	SessionContext ctx;
	ctx.Load("nonexistent/path/that/does/not/exist");
	EXPECT_FLOAT_EQ(1.0f, ctx.GetPollInterval());
	EXPECT_EQ(1000u, ctx.GetMaxLogEntries());
}

// ===========================================================================
// AssetStateTablePanel — Large dataset parsing
// ===========================================================================

TEST(AssetStateTablePanel, ParseLargeDataset_100Assets)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	for (int i = 0; i < 100; ++i)
	{
		Json::Value asset;
		char id[64];
		snprintf(id, sizeof(id), "asset.%03d", i);
		asset["id"] = id;
		asset["state"] = (i % 4 == 0) ? "Loaded" : (i % 4 == 1) ? "Staged" : (i % 4 == 2) ? "Registered" : "Unloading";
		asset["scope"] = (i % 3 == 0) ? "Stage" : "Global";
		asset["refCount"] = i;
		asset["deployPath"] = "/deploy/path";
		assets.append(asset);
	}
	data["assets"] = assets;

	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);

	EXPECT_EQ(100u, rows->Size());

	EXPECT_EQ(AssetStateEnum::kLoaded, (*rows)[0].mState);
	EXPECT_EQ(AssetScopeEnum::kStage, (*rows)[0].mScope);
	EXPECT_EQ(0u, (*rows)[0].mRefCount);

	EXPECT_EQ(AssetStateEnum::kStaged, (*rows)[1].mState);
	EXPECT_EQ(AssetScopeEnum::kGlobal, (*rows)[1].mScope);
	EXPECT_EQ(1u, (*rows)[1].mRefCount);
}

// ===========================================================================
// Integration-style tests: panel lifecycle
// ===========================================================================

TEST(AssetStateTablePanel, ActivateDeactivate_NoNull)
{
	auto panel = std::make_unique<AssetStateTablePanel>();
	panel->Activate(nullptr, nullptr, nullptr);
	panel->Update(0.016f);
	panel->OnConnectionStateChanged(true);
	panel->OnConnectionStateChanged(false);
	panel->ForceRefresh();
	panel->Deactivate();
}

TEST(StageAssetTreePanel, ActivateDeactivate_NoNull)
{
	auto panel = std::make_unique<StageAssetTreePanel>();
	panel->Activate(nullptr, nullptr, nullptr, nullptr);
	panel->Update(0.016f);
	panel->OnConnectionStateChanged(true);
	panel->OnConnectionStateChanged(false);
	panel->Deactivate();
}

TEST(RefCountInspectorPanel, ActivateDeactivate_NoNull)
{
	auto panel = std::make_unique<RefCountInspectorPanel>();
	panel->Activate(nullptr, nullptr, nullptr, nullptr);
	panel->Update(0.016f);
	panel->OnConnectionStateChanged(true);
	panel->OnConnectionStateChanged(false);
	panel->Deactivate();
}

TEST(StateTransitionLogPanel, ActivateDeactivate_NoNull)
{
	auto panel = std::make_unique<StateTransitionLogPanel>();
	panel->Activate(nullptr, nullptr, nullptr);
	panel->Update(0.016f);
	panel->OnConnectionStateChanged(true);
	panel->OnConnectionStateChanged(false);
	panel->ClearLog();
	panel->Deactivate();
}
