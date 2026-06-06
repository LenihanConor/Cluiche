// Integration tests for DiaAssetCatalogueEditorPlugin — focused on the
// get_asset_types handler and the blueprint type registrations added in
// the editor empty-state work, plus create_asset file-writing coverage.

#include <gtest/gtest.h>
#include <DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h>
#include <DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstdio>

using namespace Dia::AssetCatalogue::Editor;
using namespace Dia::EntityTemplateEditor;
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
		mBlueprintPlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mBlueprintPlugin.OnUnload();
		mPlugin.OnUnload();
		CleanTempFiles();
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

	void RegisterTempFile(const char* path)
	{
		if (mTempCount < kMaxTempFiles)
			mTempFiles[mTempCount++] = path;
	}

	void CleanTempFiles()
	{
		for (unsigned int i = 0; i < mTempCount; ++i)
			std::remove(mTempFiles[i]);
		mTempCount = 0;
	}

	DiaAssetCatalogueEditorPlugin  mPlugin;
	DiaEntityTemplateEditorPlugin  mBlueprintPlugin;
	WebUIBridge*                   mBridge = nullptr;
	EditorModel*                   mModel  = nullptr;

	static const unsigned int kMaxTempFiles = 16;
	const char*  mTempFiles[kMaxTempFiles] = {};
	unsigned int mTempCount = 0;
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

TEST_F(AssetCatalogueEditorPluginTest, GetAssetTypes_Containsdiaentitytemplate)
{
	Json::Value types = Invoke("asset_catalogue.get_asset_types")["types"];
	EXPECT_TRUE(TypePresent(types, "diaentitytemplate"));
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

// ===========================================================================
// create_asset — handler registered
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_HandlerRegistered)
{
	Json::Value r = Invoke("asset_catalogue.create_asset");
	EXPECT_FALSE(r.isNull());
	EXPECT_TRUE(r.isMember("success"));
}

// ===========================================================================
// create_asset — error paths
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_MissingParams_ReturnsError)
{
	Json::Value r = Invoke("asset_catalogue.create_asset");
	EXPECT_FALSE(r["success"].asBool());
	EXPECT_FALSE(r["error"].asString().empty());
}

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_MissingPath_ReturnsError)
{
	Json::Value data;
	data["assetType"] = "diaentitytemplate";
	data["id"]        = "diaentitytemplate.test";
	Json::Value r = Invoke("asset_catalogue.create_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_MissingId_ReturnsError)
{
	Json::Value data;
	data["assetType"]   = "diaentitytemplate";
	data["source_path"] = "itmp_create.diaentitytemplate";
	Json::Value r = Invoke("asset_catalogue.create_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_UnknownAssetType_ReturnsError)
{
	Json::Value data;
	data["assetType"]   = "unknowntype";
	data["id"]          = "unknowntype.test";
	data["source_path"] = "itmp_create_unknown.unknowntype";
	Json::Value r = Invoke("asset_catalogue.create_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}

// ===========================================================================
// create_asset — happy path: entity template
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_EntityTemplate_CreatesValidFile)
{
	const char* kPath = "itmp_create_entity.diaentitytemplate";
	RegisterTempFile(kPath);

	Json::Value data;
	data["assetType"]   = "diaentitytemplate";
	data["id"]          = "diaentitytemplate.hero";
	data["source_path"] = kPath;
	Json::Value r = Invoke("asset_catalogue.create_asset", data);

	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

// ===========================================================================
// create_asset — happy path: camera blueprint
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_Camera_UsesCorrectTopKey)
{
	const char* kPath = "itmp_create_camera.diacamera";
	RegisterTempFile(kPath);

	Json::Value data;
	data["assetType"]   = "diacamera";
	data["id"]          = "diacamera.main";
	data["source_path"] = kPath;
	Json::Value r = Invoke("asset_catalogue.create_asset", data);

	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

// ===========================================================================
// create_asset — happy path: light blueprint
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_Light_UsesCorrectTopKey)
{
	const char* kPath = "itmp_create_light.dialight";
	RegisterTempFile(kPath);

	Json::Value data;
	data["assetType"]   = "dialight";
	data["id"]          = "dialight.sun";
	data["source_path"] = kPath;
	Json::Value r = Invoke("asset_catalogue.create_asset", data);

	ASSERT_TRUE(r["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);

	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["components"].size(), 0u);
}

// ===========================================================================
// create_asset — created file is loadable and editable
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_CreatedFileIsLoadableAndEditable)
{
	const char* kPath = "itmp_create_editable.diaentitytemplate";
	RegisterTempFile(kPath);

	Json::Value createData;
	createData["assetType"]   = "diaentitytemplate";
	createData["id"]          = "diaentitytemplate.npc";
	createData["source_path"] = kPath;
	ASSERT_TRUE(Invoke("asset_catalogue.create_asset", createData)["success"].asBool());

	Json::Value addData;
	addData["path"]          = kPath;
	addData["componentType"] = "Transform2D";
	ASSERT_TRUE(Invoke("entity_template_editor.add_component", addData)["success"].asBool());

	Json::Value loadData;
	loadData["path"] = kPath;
	Json::Value loaded = Invoke("entity_template_editor.load", loadData);
	ASSERT_TRUE(loaded["success"].asBool());
	EXPECT_EQ(loaded["properties"]["components"].size(), 1u);
	EXPECT_EQ(loaded["properties"]["components"][0]["type"].asString(), "Transform2D");
}

// ===========================================================================
// create_asset — invalid directory returns error
// ===========================================================================

TEST_F(AssetCatalogueEditorPluginTest, CreateAsset_InvalidDirectory_ReturnsError)
{
	Json::Value data;
	data["assetType"]   = "diaentitytemplate";
	data["id"]          = "diaentitytemplate.test";
	data["source_path"] = "nonexistent_dir_xyz/sub/file.diaentitytemplate";
	Json::Value r = Invoke("asset_catalogue.create_asset", data);
	EXPECT_FALSE(r["success"].asBool());
}
