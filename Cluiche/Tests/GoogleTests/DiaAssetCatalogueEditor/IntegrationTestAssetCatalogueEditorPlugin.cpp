// Integration tests for DiaAssetCatalogueEditorPlugin — focused on the
// get_asset_types handler and the blueprint type registrations added in
// the editor empty-state work.

#include <gtest/gtest.h>
#include <DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::AssetCatalogue::Editor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Fixture
// ===========================================================================

class AssetCatalogueEditorPluginTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mBridge = new WebUIBridge(nullptr);
		mModel  = new EditorModel();

		EditorPluginContext ctx;
		ctx.mBridge       = mBridge;
		ctx.mModel        = mModel;
		ctx.mView         = nullptr;
		ctx.mPluginLoader = nullptr;
		ctx.mServices     = nullptr;

		mPlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mPlugin.OnUnload();
		delete mModel;
		delete mBridge;
	}

	Json::Value Invoke(const char* command,
	                   const Json::Value& data = Json::Value(Json::objectValue))
	{
		return mBridge->InvokeRequestHandler(StringCRC(command), data);
	}

	bool TypePresent(const Json::Value& types, const char* typeId)
	{
		for (unsigned int i = 0; i < types.size(); ++i)
			if (types[i]["typeId"].asString() == typeId) return true;
		return false;
	}

	DiaAssetCatalogueEditorPlugin  mPlugin;
	WebUIBridge*                   mBridge = nullptr;
	EditorModel*                   mModel  = nullptr;
};

// ===========================================================================
// get_asset_types — handler registered
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_HandlerRegistered)
{
	Json::Value r = Invoke("asset_catalogue.get_asset_types");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ReturnsSuccess)
{
	Json::Value r = Invoke("asset_catalogue.get_asset_types");
	EXPECT_TRUE(r["success"].asBool());
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ReturnsTypesArray)
{
	Json::Value r = Invoke("asset_catalogue.get_asset_types");
	EXPECT_TRUE(r["types"].isArray());
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_TypesArrayNonEmpty)
{
	Json::Value r = Invoke("asset_catalogue.get_asset_types");
	EXPECT_GT(r["types"].size(), 0u);
}

// ===========================================================================
// get_asset_types — built-in types present
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsTexture)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "texture"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsConfig)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "config"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsStage)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "stage"));
}

// ===========================================================================
// get_asset_types — blueprint types present (added in editor empty-state work)
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsDiaEntity)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "diaentity"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsDiaCamera)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "diacamera"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_ContainsDiaLight)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "dialight"));
}

// ===========================================================================
// get_asset_types — each entry has required fields for New dialog
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_EachEntryHasTypeIdAndName)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	for (unsigned int i = 0; i < types.size(); ++i)
	{
		EXPECT_TRUE(types[i].isMember("typeId")) << "Missing typeId at index " << i;
		EXPECT_TRUE(types[i].isMember("name"))   << "Missing name at index "   << i;
		EXPECT_FALSE(types[i]["typeId"].asString().empty()) << "Empty typeId at index " << i;
		EXPECT_FALSE(types[i]["name"].asString().empty())   << "Empty name at index "   << i;
	}
}

// ===========================================================================
// OnUnload removes the handler
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, OnUnload_RemovesGetAssetTypesHandler)
{
	mPlugin.OnUnload();
	EXPECT_TRUE(Invoke("asset_catalogue.get_asset_types").isNull());
	// Re-load so TearDown doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge = mBridge;
	ctx.mModel  = mModel;
	mPlugin.OnLoad(ctx);
}

// ===========================================================================
// get_state — handler registered and returns required fields
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, GetState_HandlerRegistered)
{
	Json::Value r = Invoke("asset_catalogue.get_state");
	EXPECT_FALSE(r.isNull());
}

TEST_F(AssetCatalogueEditorPluginTest, GetState_NoProjectLoaded_HasRequiredFields)
{
	Json::Value r = Invoke("asset_catalogue.get_state");
	EXPECT_TRUE(r.isMember("success"));
	EXPECT_TRUE(r.isMember("path"));
	EXPECT_TRUE(r.isMember("records"));
	EXPECT_TRUE(r.isMember("dirty"));
}

TEST_F(AssetCatalogueEditorPluginTest, GetState_NoProjectLoaded_ReturnsSuccessWithEmptyState)
{
	Json::Value r = Invoke("asset_catalogue.get_state");
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["path"].asString(), "");
	EXPECT_TRUE(r["records"].isArray());
	EXPECT_EQ(r["records"].size(), 0u);
}

TEST_F(AssetCatalogueEditorPluginTest, GetState_NoProjectLoaded_HasStatusHint)
{
	// When no catalogue is loaded the plugin returns a status hint for the UI.
	Json::Value r = Invoke("asset_catalogue.get_state");
	EXPECT_TRUE(r.isMember("status"));
	EXPECT_FALSE(r["status"].asString().empty());
}

TEST_F(AssetCatalogueEditorPluginTest, OnUnload_RemovesGetStateHandler)
{
	mPlugin.OnUnload();
	EXPECT_TRUE(Invoke("asset_catalogue.get_state").isNull());
	// Re-load so TearDown doesn't double-unregister
	EditorPluginContext ctx;
	ctx.mBridge = mBridge;
	ctx.mModel  = mModel;
	mPlugin.OnLoad(ctx);
}
