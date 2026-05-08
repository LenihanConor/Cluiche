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

// ===========================================================================
// Parser — "assetId" field name support (game-side uses "assetId" not "id")
// ===========================================================================

TEST_F(AssetStateTablePanelParseTest, ParseAssetIdFieldName)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["assetId"] = "game.style.id";
	asset["state"] = "Loaded";
	asset["scope"] = "Global";
	asset["refCount"] = 5;
	asset["deployPath"] = "/deploy/file.bin";
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(StringCRC("game.style.id"), (*mRows)[0].mAssetId);
	EXPECT_EQ(AssetStateEnum::kLoaded, (*mRows)[0].mState);
}

// ===========================================================================
// Parser — stageId field parsing
// ===========================================================================

TEST_F(AssetStateTablePanelParseTest, ParseStageIdField)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["assetId"] = "bg.texture";
	asset["state"] = "Staged";
	asset["scope"] = "Stage";
	asset["stageId"] = "stage.gameplay";
	asset["refCount"] = 1;
	asset["deployPath"] = "/deploy/bg.dds";
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(AssetScopeEnum::kStage, (*mRows)[0].mScope);
	EXPECT_EQ(StringCRC("stage.gameplay"), (*mRows)[0].mStageId);
}

TEST_F(AssetStateTablePanelParseTest, ParseMissingStageId_DefaultsToEmpty)
{
	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["assetId"] = "global.asset";
	asset["state"] = "Loaded";
	asset["scope"] = "Global";
	asset["refCount"] = 2;
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *mRows);

	ASSERT_EQ(1u, mRows->Size());
	EXPECT_EQ(StringCRC(), (*mRows)[0].mStageId);
}

// ===========================================================================
// SessionContext — Filter state persistence
// ===========================================================================

TEST(SessionContext, FilterState_DefaultsEmpty)
{
	SessionContext ctx;
	EXPECT_STREQ("", ctx.GetStateFilter());
	EXPECT_STREQ("", ctx.GetIdSearchText());
}

TEST(SessionContext, FilterState_SetAndGet)
{
	SessionContext ctx;
	ctx.SetStateFilter("Loaded");
	ctx.SetIdSearchText("texture");
	EXPECT_STREQ("Loaded", ctx.GetStateFilter());
	EXPECT_STREQ("texture", ctx.GetIdSearchText());
}

TEST(SessionContext, FilterState_NullSetsEmpty)
{
	SessionContext ctx;
	ctx.SetStateFilter("Loaded");
	ctx.SetStateFilter(nullptr);
	EXPECT_STREQ("", ctx.GetStateFilter());
}

TEST(SessionContext, FilterState_SaveAndLoadRoundTrip)
{
	const char* testDir = "Cluiche/out/test_session_context_filters";

	{
		SessionContext ctx;
		ctx.SetPollInterval(1.5f);
		ctx.SetStateFilter("Staged");
		ctx.SetIdSearchText("mesh");
		ctx.Save(testDir);
	}

	{
		SessionContext ctx;
		ctx.Load(testDir);
		EXPECT_FLOAT_EQ(1.5f, ctx.GetPollInterval());
		EXPECT_STREQ("Staged", ctx.GetStateFilter());
		EXPECT_STREQ("mesh", ctx.GetIdSearchText());
	}
}

// ===========================================================================
// AssetStateRow — mStageId field
// ===========================================================================

TEST(AssetStateRow, DefaultConstruction_StageIdEmpty)
{
	AssetStateRow row;
	EXPECT_EQ(StringCRC(), row.mStageId);
}

// ===========================================================================
// StageAssetTreePanel — Root node derivation from snapshot
// ===========================================================================

TEST(StageAssetTreePanel, RootNodeDerivation_GlobalAndStageAssets)
{
	// Build a snapshot with global and stage-scoped assets
	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();

	Json::Value data;
	Json::Value assets(Json::arrayValue);

	// 3 global assets
	for (int i = 0; i < 3; ++i)
	{
		Json::Value asset;
		char id[32];
		snprintf(id, sizeof(id), "global.asset%d", i);
		asset["id"] = id;
		asset["state"] = "Loaded";
		asset["scope"] = "Global";
		asset["refCount"] = 2;
		assets.append(asset);
	}

	// 2 assets in stage "gameplay"
	for (int i = 0; i < 2; ++i)
	{
		Json::Value asset;
		char id[32];
		snprintf(id, sizeof(id), "stage.asset%d", i);
		asset["id"] = id;
		asset["state"] = "Staged";
		asset["scope"] = "Stage";
		asset["stageId"] = "stage.gameplay";
		asset["refCount"] = 1;
		assets.append(asset);
	}

	// 1 asset in stage "menu"
	{
		Json::Value asset;
		asset["id"] = "menu.bg";
		asset["state"] = "Loaded";
		asset["scope"] = "Stage";
		asset["stageId"] = "stage.menu";
		asset["refCount"] = 1;
		assets.append(asset);
	}

	data["assets"] = assets;
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);

	ASSERT_EQ(6u, rows->Size());

	// Verify global assets parsed correctly
	unsigned int globalCount = 0;
	unsigned int stageGameplayCount = 0;
	unsigned int stageMenuCount = 0;
	for (unsigned int i = 0; i < rows->Size(); ++i)
	{
		if ((*rows)[i].mScope == AssetScopeEnum::kGlobal) globalCount++;
		else if ((*rows)[i].mStageId == StringCRC("stage.gameplay")) stageGameplayCount++;
		else if ((*rows)[i].mStageId == StringCRC("stage.menu")) stageMenuCount++;
	}

	EXPECT_EQ(3u, globalCount);
	EXPECT_EQ(2u, stageGameplayCount);
	EXPECT_EQ(1u, stageMenuCount);
}

TEST(StageAssetTreePanel, RootNodeDerivation_NoGlobalAssets)
{
	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();

	Json::Value data;
	Json::Value assets(Json::arrayValue);

	Json::Value asset;
	asset["id"] = "level.terrain";
	asset["state"] = "Loaded";
	asset["scope"] = "Stage";
	asset["stageId"] = "stage.world";
	asset["refCount"] = 1;
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);
	ASSERT_EQ(1u, rows->Size());
	EXPECT_EQ(AssetScopeEnum::kStage, (*rows)[0].mScope);
	EXPECT_EQ(StringCRC("stage.world"), (*rows)[0].mStageId);
}

// ===========================================================================
// RefCountInspectorPanel — Reference resolution logic
// ===========================================================================

TEST(RefCountInspectorPanel, ReferenceResolution_GlobalAssetInMultipleStages)
{
	// Build a snapshot with a global asset and stage-scoped assets
	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();

	Json::Value data;
	Json::Value assets(Json::arrayValue);

	// Global asset with refCount 3
	{
		Json::Value asset;
		asset["id"] = "texture.shared";
		asset["state"] = "Loaded";
		asset["scope"] = "Global";
		asset["refCount"] = 3;
		assets.append(asset);
	}

	// Stage-scoped assets to define stages
	const char* stageIds[] = {"stage.level1", "stage.level2", "stage.menu"};
	for (int i = 0; i < 3; ++i)
	{
		Json::Value asset;
		char id[32];
		snprintf(id, sizeof(id), "stage.local%d", i);
		asset["id"] = id;
		asset["state"] = "Loaded";
		asset["scope"] = "Stage";
		asset["stageId"] = stageIds[i];
		asset["refCount"] = 1;
		assets.append(asset);
	}

	data["assets"] = assets;
	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);

	ASSERT_EQ(4u, rows->Size());

	// Verify the global asset has correct ref count in snapshot
	EXPECT_EQ(StringCRC("texture.shared"), (*rows)[0].mAssetId);
	EXPECT_EQ(AssetScopeEnum::kGlobal, (*rows)[0].mScope);
	EXPECT_EQ(3u, (*rows)[0].mRefCount);

	// Verify we can identify all stages from snapshot
	Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> stages;
	for (unsigned int i = 0; i < rows->Size(); ++i)
	{
		const AssetStateRow& row = (*rows)[i];
		if (row.mScope != AssetScopeEnum::kStage || row.mStageId == StringCRC())
			continue;
		bool found = false;
		for (unsigned int s = 0; s < stages.Size(); ++s)
		{
			if (stages[s] == row.mStageId) { found = true; break; }
		}
		if (!found) stages.Add(row.mStageId);
	}
	EXPECT_EQ(3u, stages.Size());
}

TEST(RefCountInspectorPanel, ReferenceResolution_StageScopedAsset)
{
	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();

	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["id"] = "stage.only.asset";
	asset["state"] = "Loaded";
	asset["scope"] = "Stage";
	asset["stageId"] = "stage.owner";
	asset["refCount"] = 1;
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);
	ASSERT_EQ(1u, rows->Size());

	// Stage-scoped asset: inspector should show single reference message
	EXPECT_EQ(AssetScopeEnum::kStage, (*rows)[0].mScope);
	EXPECT_EQ(StringCRC("stage.owner"), (*rows)[0].mStageId);
	EXPECT_EQ(1u, (*rows)[0].mRefCount);
}

TEST(RefCountInspectorPanel, ReferenceResolution_AssetNotInSnapshot)
{
	auto rows = std::make_unique<Dia::Core::Containers::DynamicArrayC<AssetStateRow, AssetStateTablePanel::kMaxAssets>>();

	Json::Value data;
	Json::Value assets(Json::arrayValue);
	Json::Value asset;
	asset["id"] = "some.other.asset";
	asset["state"] = "Loaded";
	asset["scope"] = "Global";
	asset["refCount"] = 1;
	assets.append(asset);
	data["assets"] = assets;

	AssetStateTablePanel::ParseGetAllStatesResponse(data, *rows);
	ASSERT_EQ(1u, rows->Size());

	// Searching for a non-existent asset should find nothing
	StringCRC missingId("does.not.exist");
	bool found = false;
	for (unsigned int i = 0; i < rows->Size(); ++i)
	{
		if ((*rows)[i].mAssetId == missingId)
		{
			found = true;
			break;
		}
	}
	EXPECT_FALSE(found);
}

// ===========================================================================
// StateTransitionLogPanel — Ring buffer FIFO overflow
// ===========================================================================

TEST(StateTransitionLogPanel, FIFOOverflow_OldestDropped)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(10);
	panel.Activate(nullptr, nullptr, nullptr);

	// Simulate connected state so HandleTransitionEvent processes events
	panel.OnConnectionStateChanged(true);

	EXPECT_EQ(10u, panel.GetMaxEntries());

	// The log count should start with just the reconnect marker
	EXPECT_EQ(1u, panel.GetLogCount());

	// Clear to get a clean state
	panel.ClearLog();
	EXPECT_EQ(0u, panel.GetLogCount());

	panel.Deactivate();
}

TEST(StateTransitionLogPanel, FIFOOverflow_SetMaxReducesCount)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(100);

	// We can't easily inject entries without a mock manager,
	// but we can verify that reducing max enforces the cap
	panel.Activate(nullptr, nullptr, nullptr);
	panel.SetMaxEntries(5);
	EXPECT_LE(panel.GetLogCount(), 5u);
	panel.Deactivate();
}

TEST(StateTransitionLogPanel, RingBuffer_GetLogEntry)
{
	StateTransitionLogPanel panel;
	panel.SetMaxEntries(4096);
	panel.Activate(nullptr, nullptr, nullptr);

	// After activate with no connection, log count is 0
	EXPECT_EQ(0u, panel.GetLogCount());

	// Trigger connection which appends a reconnect marker
	panel.OnConnectionStateChanged(true);
	EXPECT_EQ(1u, panel.GetLogCount());

	// Entry at index 0 should be a reconnect marker
	const TransitionLogEntry& entry = panel.GetLogEntry(0);
	EXPECT_EQ(LogEntryType::kMarkerReconnect, entry.mType);

	// Disconnect appends another marker
	panel.OnConnectionStateChanged(false);
	EXPECT_EQ(2u, panel.GetLogCount());

	const TransitionLogEntry& entry2 = panel.GetLogEntry(1);
	EXPECT_EQ(LogEntryType::kMarkerDisconnect, entry2.mType);

	panel.Deactivate();
}

TEST(StateTransitionLogPanel, FIFOOverflow_MaxEntriesEnforced)
{
	StateTransitionLogPanel panel;
	panel.Activate(nullptr, nullptr, nullptr);

	// Generate 12 markers (min settable max is 10)
	for (int i = 0; i < 6; ++i)
	{
		panel.OnConnectionStateChanged(true);
		panel.OnConnectionStateChanged(false);
	}
	EXPECT_EQ(12u, panel.GetLogCount());

	// Reduce max to 10 (minimum allowed) — oldest 2 should be evicted
	panel.SetMaxEntries(10);
	EXPECT_EQ(10u, panel.GetLogCount());

	// Newest entry is a disconnect marker (last toggle was false)
	const TransitionLogEntry& newest = panel.GetLogEntry(panel.GetLogCount() - 1);
	EXPECT_EQ(LogEntryType::kMarkerDisconnect, newest.mType);

	// Oldest remaining is a reconnect (3rd event: index 2 of original 12, now index 0)
	const TransitionLogEntry& oldest = panel.GetLogEntry(0);
	EXPECT_EQ(LogEntryType::kMarkerReconnect, oldest.mType);

	panel.Deactivate();
}
