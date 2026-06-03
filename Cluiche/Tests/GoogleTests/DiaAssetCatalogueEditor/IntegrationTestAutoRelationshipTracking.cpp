// Integration tests for Auto-Relationship Tracking.
// Covers two plugins sharing one WebUIBridge:
//   - DiaAssetCatalogueEditorPlugin (owns relationship + infer handlers)
//   - DiaSceneEditorPlugin (calls add/remove relationship after scene mutations)

#include <gtest/gtest.h>
#include <DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h>
#include <DiaSceneEditor/DiaSceneEditorPlugin.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <sstream>
#include <string>
#include <cstdio>

using namespace Dia::AssetCatalogue::Editor;
using namespace Dia::SceneEditor;
using namespace Dia::Editor;
using namespace Dia::Core;

// ===========================================================================
// Temp file paths (written relative to CWD; cleaned up in TearDown)
// ===========================================================================

static const char* kTempScenePath  = "test_autorel_scene.diascene";
static const char* kTempScene2Path = "test_autorel_scene2.diascene";

// ===========================================================================
// Fixture
// ===========================================================================

class AutoRelTrackingTest : public ::testing::Test
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

		// Catalogue plugin MUST load first — it owns the relationship handlers
		mCataloguePlugin.OnLoad(ctx);
		mScenePlugin.OnLoad(ctx);
	}

	void TearDown() override
	{
		mScenePlugin.OnUnload();
		mCataloguePlugin.OnUnload();
		delete mModel;
		delete mBridge;

		remove(kTempScenePath);
		remove(kTempScene2Path);
	}

	// -----------------------------------------------------------------------
	// Core invoke helper
	// -----------------------------------------------------------------------

	Json::Value Invoke(const char* command,
	                   const Json::Value& data = Json::Value(Json::objectValue))
	{
		return mBridge->InvokeRequestHandler(StringCRC(command), data);
	}

	// -----------------------------------------------------------------------
	// CreateRecord — calls asset_catalogue.create_record
	// -----------------------------------------------------------------------

	void CreateRecord(const char* id, const char* type, const char* sourcePath)
	{
		Json::Value req;
		req["id"]          = id;
		req["type"]        = type;
		req["source_path"] = sourcePath;
		req["status"]      = "Active";
		Invoke("asset_catalogue.create_record", req);
	}

	// -----------------------------------------------------------------------
	// GetForwardRefs — returns the refs[] array from asset_catalogue.get_forward_refs
	// -----------------------------------------------------------------------

	Json::Value GetForwardRefs(const char* id)
	{
		Json::Value req;
		req["id"] = id;
		Json::Value r = Invoke("asset_catalogue.get_forward_refs", req);
		if (r.isNull() || !r.isMember("refs"))
			return Json::Value(Json::arrayValue);
		return r["refs"];
	}

	// -----------------------------------------------------------------------
	// HasRef — true if refs array contains a matching rel+target entry
	// -----------------------------------------------------------------------

	bool HasRef(const Json::Value& refs, const char* rel, const char* target)
	{
		for (unsigned int i = 0; i < refs.size(); ++i)
		{
			if (refs[i]["rel"].asString()    == rel &&
			    refs[i]["target"].asString() == target)
				return true;
		}
		return false;
	}

	// -----------------------------------------------------------------------
	// WriteTempSceneFile — writes a .diascene with scene2d structure.
	// Pass nullptr or "" to skip that array entry.
	// -----------------------------------------------------------------------

	void WriteTempSceneFile(const char* path,
	                        const char* entityBp  = nullptr,
	                        const char* cameraBp  = nullptr,
	                        const char* lightBp   = nullptr)
	{
		Json::Value root;
		Json::Value scene2d(Json::objectValue);
		Json::Value entities(Json::arrayValue);
		Json::Value cameras(Json::arrayValue);
		Json::Value lights(Json::arrayValue);

		auto makeItem = [](const char* bp) -> Json::Value
		{
			Json::Value item(Json::objectValue);
			item["id"]["value"]        = "item_0";
			item["blueprint"]["value"] = bp;
			item["enabled"]            = true;
			item["instance_data"]      = Json::Value(Json::objectValue);
			return item;
		};

		if (entityBp && entityBp[0]) entities.append(makeItem(entityBp));
		if (cameraBp && cameraBp[0]) cameras.append(makeItem(cameraBp));
		if (lightBp  && lightBp[0])  lights.append(makeItem(lightBp));

		scene2d["entities"] = entities;
		scene2d["cameras"]  = cameras;
		scene2d["lights"]   = lights;
		scene2d["layers"]   = Json::Value(Json::arrayValue);
		root["scene2d"]     = scene2d;

		Json::StreamWriterBuilder b;
		std::string content = Json::writeString(b, root);
		FILE* f = nullptr;
		fopen_s(&f, path, "wb");
		if (f)
		{
			fwrite(content.c_str(), 1, content.size(), f);
			fclose(f);
		}
	}

	DiaAssetCatalogueEditorPlugin  mCataloguePlugin;
	DiaSceneEditorPlugin           mScenePlugin;
	WebUIBridge*                   mBridge = nullptr;
	EditorModel*                   mModel  = nullptr;
};

// ===========================================================================
// infer_relationships — handler registration
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_HandlerRegistered)
{
	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	EXPECT_FALSE(r.isNull());
}

// ===========================================================================
// infer_relationships — empty catalogue
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_EmptyRegistry_ReturnsZeroCounts)
{
	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r.isNull());
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["scenes_scanned"].asInt(), 0);
	EXPECT_EQ(r["edges_added"].asInt(),    0);
	EXPECT_EQ(r["edges_skipped"].asInt(),  0);
}

// ===========================================================================
// infer_relationships — non-scene record is not scanned
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_NoDiasceneRecords_ScannedZero)
{
	CreateRecord("tex.hero", "texture", "hero.png");

	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r.isNull());
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["scenes_scanned"].asInt(), 0);
}

// ===========================================================================
// infer_relationships — scene with a valid blueprint ref adds one edge
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_SceneWithBlueprintRef_AddsEdge)
{
	WriteTempSceneFile(kTempScenePath, "diaentity.hero");
	CreateRecord("diascene.test_scene",  "diascene",   kTempScenePath);
	CreateRecord("diaentity.hero",       "diaentity",  "hero.diaentity");

	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r.isNull());
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["edges_added"].asInt(), 1);

	Json::Value refs = GetForwardRefs("diascene.test_scene");
	EXPECT_TRUE(HasRef(refs, "uses", "diaentity.hero"));
}

// ===========================================================================
// infer_relationships — running twice does not create duplicate edges
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_Idempotent_NoDuplicateEdge)
{
	WriteTempSceneFile(kTempScenePath, "diaentity.hero");
	CreateRecord("diascene.test_scene",  "diascene",   kTempScenePath);
	CreateRecord("diaentity.hero",       "diaentity",  "hero.diaentity");

	Invoke("asset_catalogue.infer_relationships");

	Json::Value r2 = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r2.isNull());
	EXPECT_TRUE(r2["success"].asBool());
	// Second run: edge already exists → skipped, not added again
	EXPECT_EQ(r2["edges_added"].asInt(),   0);
	EXPECT_EQ(r2["edges_skipped"].asInt(), 1);

	// Still only one ref in the registry
	Json::Value refs = GetForwardRefs("diascene.test_scene");
	int useCount = 0;
	for (unsigned int i = 0; i < refs.size(); ++i)
		if (refs[i]["rel"].asString() == "uses" && refs[i]["target"].asString() == "diaentity.hero")
			++useCount;
	EXPECT_EQ(useCount, 1);
}

// ===========================================================================
// infer_relationships — unknown blueprint is skipped
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_UnknownBlueprint_SkipsEdge)
{
	WriteTempSceneFile(kTempScenePath, "diaentity.nonexistent");
	// Only register the scene record — no matching blueprint record
	CreateRecord("diascene.test_scene", "diascene", kTempScenePath);

	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r.isNull());
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["edges_added"].asInt(),   0);
	EXPECT_EQ(r["edges_skipped"].asInt(), 1);
}

// ===========================================================================
// infer_relationships — multiple item types each produce an edge
// ===========================================================================

TEST_F(AutoRelTrackingTest, InferRelationships_MultipleItemTypes_AllEdgesAdded)
{
	WriteTempSceneFile(kTempScenePath, "diaentity.hero", "diacamera.main_cam", "dialight.sun");
	CreateRecord("diascene.test_scene",    "diascene",   kTempScenePath);
	CreateRecord("diaentity.hero",         "diaentity",  "hero.diaentity");
	CreateRecord("diacamera.main_cam",     "diacamera",  "main_cam.diacamera");
	CreateRecord("dialight.sun",           "dialight",   "sun.dialight");

	Json::Value r = Invoke("asset_catalogue.infer_relationships");
	ASSERT_FALSE(r.isNull());
	EXPECT_TRUE(r["success"].asBool());
	EXPECT_EQ(r["edges_added"].asInt(), 3);
}

// ===========================================================================
// scene_editor.add_item → catalogue relationship registered
// ===========================================================================

TEST_F(AutoRelTrackingTest, AddItem_WithCatalogueId_AddsForwardRef)
{
	// Write a blank scene and create matching catalogue record
	WriteTempSceneFile(kTempScenePath);
	CreateRecord("diascene.ar_scene", "diascene",  kTempScenePath);
	CreateRecord("diaentity.box",     "diaentity", "box.diaentity");

	// Load scene into DiaSceneEditorPlugin (triggers ResolveCatalogueIdForLoadedScene)
	Json::Value loadReq;
	loadReq["path"] = kTempScenePath;
	Json::Value loadResult = Invoke("scene_editor.load_scene", loadReq);
	ASSERT_TRUE(loadResult["success"].asBool()) << "load_scene failed";

	// Add entity — should register a "uses" edge in the catalogue
	Json::Value addReq;
	addReq["itemType"]    = "entity";
	addReq["blueprintId"] = "diaentity.box";
	Json::Value addResult = Invoke("scene_editor.add_item", addReq);
	ASSERT_TRUE(addResult["success"].asBool()) << "add_item failed";

	Json::Value refs = GetForwardRefs("diascene.ar_scene");
	EXPECT_TRUE(HasRef(refs, "uses", "diaentity.box"));
}

// ===========================================================================
// scene_editor.delete_item → catalogue relationship removed
// ===========================================================================

TEST_F(AutoRelTrackingTest, DeleteItem_AfterAdd_RemovesForwardRef)
{
	WriteTempSceneFile(kTempScenePath);
	CreateRecord("diascene.ar_scene", "diascene",  kTempScenePath);
	CreateRecord("diaentity.box",     "diaentity", "box.diaentity");

	Json::Value loadReq;
	loadReq["path"] = kTempScenePath;
	Invoke("scene_editor.load_scene", loadReq);

	// Add then delete the item
	Json::Value addReq;
	addReq["itemType"]    = "entity";
	addReq["blueprintId"] = "diaentity.box";
	Invoke("scene_editor.add_item", addReq);

	// SceneMutator generates id as "{blueprintId}_{N}" → "diaentity.box_0"
	Json::Value delReq;
	delReq["itemType"] = "entity";
	delReq["itemId"]   = "diaentity.box_0";
	Json::Value delResult = Invoke("scene_editor.delete_item", delReq);
	ASSERT_TRUE(delResult["success"].asBool()) << "delete_item failed";

	Json::Value refs = GetForwardRefs("diascene.ar_scene");
	EXPECT_FALSE(HasRef(refs, "uses", "diaentity.box"));
}

// ===========================================================================
// scene_editor.change_blueprint → old ref removed, new ref added
// ===========================================================================

TEST_F(AutoRelTrackingTest, ChangeBlueprint_UpdatesForwardRef)
{
	WriteTempSceneFile(kTempScenePath);
	CreateRecord("diascene.ar_scene", "diascene",  kTempScenePath);
	CreateRecord("diaentity.old",     "diaentity", "old.diaentity");
	CreateRecord("diaentity.new_bp",  "diaentity", "new_bp.diaentity");

	Json::Value loadReq;
	loadReq["path"] = kTempScenePath;
	Invoke("scene_editor.load_scene", loadReq);

	// Add entity with old blueprint
	Json::Value addReq;
	addReq["itemType"]    = "entity";
	addReq["blueprintId"] = "diaentity.old";
	Invoke("scene_editor.add_item", addReq);

	// Change to new blueprint
	Json::Value changeReq;
	changeReq["itemType"]             = "entity";
	changeReq["itemId"]               = "diaentity.old_0";
	changeReq["newBlueprintId"]       = "diaentity.new_bp";
	changeReq["newBlueprintComponents"] = Json::Value(Json::arrayValue);
	Json::Value changeResult = Invoke("scene_editor.change_blueprint", changeReq);
	ASSERT_TRUE(changeResult["success"].asBool()) << "change_blueprint failed";

	Json::Value refs = GetForwardRefs("diascene.ar_scene");
	EXPECT_FALSE(HasRef(refs, "uses", "diaentity.old"))   << "old blueprint ref should be removed";
	EXPECT_TRUE(HasRef(refs,  "uses", "diaentity.new_bp")) << "new blueprint ref should be present";
}

// ===========================================================================
// add_item with no matching catalogue record — no relationship registered
// ===========================================================================

TEST_F(AutoRelTrackingTest, AddItem_NoCatalogueIdCached_NoRelationship)
{
	// Write a scene file but do NOT create a catalogue record for it.
	// ResolveCatalogueIdForLoadedScene will find no match → mSceneCatalogueId stays empty.
	WriteTempSceneFile(kTempScenePath);
	CreateRecord("diaentity.box", "diaentity", "box.diaentity");

	Json::Value loadReq;
	loadReq["path"] = kTempScenePath;
	Json::Value loadResult = Invoke("scene_editor.load_scene", loadReq);
	ASSERT_TRUE(loadResult["success"].asBool()) << "load_scene failed";

	Json::Value addReq;
	addReq["itemType"]    = "entity";
	addReq["blueprintId"] = "diaentity.box";
	Json::Value addResult = Invoke("scene_editor.add_item", addReq);
	ASSERT_TRUE(addResult["success"].asBool()) << "add_item failed";

	// Since no catalogue ID was resolved, no relationship should have been registered.
	// get_forward_refs on the blueprint itself should return empty (it is a target, not a source).
	Json::Value refs = GetForwardRefs("diaentity.box");
	EXPECT_EQ(refs.size(), 0u) << "no forward refs should exist when no catalogue ID was resolved";
}
