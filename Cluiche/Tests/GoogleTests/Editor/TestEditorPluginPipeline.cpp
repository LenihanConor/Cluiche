#include <gtest/gtest.h>
#include <DiaEditor/EditorManifestLoader.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/HelloEditorPlugin.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaCore/CRC/StringCRC.h>
#include <fstream>
#include <cstdio>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
	const char* kTempManifest = "pipeline_test.diaapp";

	void WriteManifest(const char* content)
	{
		std::ofstream f(kTempManifest);
		f << content;
	}

	struct PipelineResult
	{
		IEditorPlugin* plugins[8];
		unsigned int count;
	};

	void CollectPlugins(const EditorManifestLoader::PluginEntry& entry, void* userData)
	{
		auto* result = static_cast<PipelineResult*>(userData);
		if (result->count >= 8)
			return;

		IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(StringCRC(entry.typeId));
		result->plugins[result->count++] = plugin;
	}
}

TEST(EditorPluginPipeline, FullPipeline_ManifestToOnLoad)
{
	WriteManifest(R"({ "editor": { "enabled": true, "plugins": [{ "type": "HelloEditorPlugin", "instance_id": "test_hello" }] } })");

	PipelineResult result = {};
	EditorManifestLoader::Load(kTempManifest, CollectPlugins, &result);

	ASSERT_EQ(result.count, 1u);
	ASSERT_NE(result.plugins[0], nullptr);
	EXPECT_STREQ(result.plugins[0]->GetName(), "HelloEditorPlugin");

	EditorModel model;
	EditorPluginContext context;
	context.mModel = &model;
	result.plugins[0]->OnLoad(context);

	HelloEditorPlugin* hello = static_cast<HelloEditorPlugin*>(result.plugins[0]);
	EXPECT_TRUE(hello->IsLoaded());

	hello->OnUnload();
	EXPECT_FALSE(hello->IsLoaded());

	delete result.plugins[0];
	std::remove(kTempManifest);
}

TEST(EditorPluginPipeline, FullPipeline_MultiplePlugins)
{
	WriteManifest(R"({ "editor": { "enabled": true, "plugins": [
		{ "type": "StubEditorPlugin", "instance_id": "s1" },
		{ "type": "HelloEditorPlugin", "instance_id": "h1" }
	] } })");

	PipelineResult result = {};
	EditorManifestLoader::Load(kTempManifest, CollectPlugins, &result);

	ASSERT_EQ(result.count, 2u);
	ASSERT_NE(result.plugins[0], nullptr);
	ASSERT_NE(result.plugins[1], nullptr);
	EXPECT_STREQ(result.plugins[0]->GetName(), "StubEditorPlugin");
	EXPECT_STREQ(result.plugins[1]->GetName(), "HelloEditorPlugin");

	delete result.plugins[0];
	delete result.plugins[1];
	std::remove(kTempManifest);
}

TEST(EditorPluginPipeline, FullPipeline_UnknownTypeSkipped)
{
	WriteManifest(R"({ "editor": { "enabled": true, "plugins": [{ "type": "NonExistentPlugin_XYZ", "instance_id": "n1" }] } })");

	PipelineResult result = {};
	EditorManifestLoader::Load(kTempManifest, CollectPlugins, &result);

	ASSERT_EQ(result.count, 1u);
	EXPECT_EQ(result.plugins[0], nullptr);

	std::remove(kTempManifest);
}

TEST(EditorPluginPipeline, FullPipeline_DisabledEditorSkipped)
{
	WriteManifest(R"({ "editor": { "enabled": false, "plugins": [{ "type": "HelloEditorPlugin", "instance_id": "h1" }] } })");

	PipelineResult result = {};
	EditorManifestLoader::Load(kTempManifest, CollectPlugins, &result);

	EXPECT_EQ(result.count, 0u);

	std::remove(kTempManifest);
}
