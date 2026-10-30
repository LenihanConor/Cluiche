#include <gtest/gtest.h>
#include <DiaEditor/Plugin/EditorPluginBase.h>

using namespace Dia::Editor;

namespace
{
	class PlainPlugin : public IEditorPlugin
	{
	public:
		const char* GetName() const override { return "Plain"; }
		const char* GetVersion() const override { return "1.0"; }
		const char* GetDescription() const override { return ""; }
		const char* GetUIPath() const override { return ""; }
		LayoutMode GetLayoutMode() const override { return LayoutMode::kHeadless; }
		void OnLoad(const EditorPluginContext&) override {}
		void OnUnload() override {}
		void OnUpdate(float) override {}
	};

	class DirtyTestPlugin : public EditorPluginBase
	{
	public:
		DirtyTestPlugin()
			: EditorPluginBase({ "DirtyTest", "1.0", "d", "dia://x", LayoutMode::kHeadless, nullptr, nullptr, false })
		{}

		void TestMarkDirty() { MarkDirty(); }
		void TestClearDirty() { ClearDirty(); }
	};

	class SavingPlugin : public DirtyTestPlugin
	{
	public:
		bool Save() override
		{
			ClearDirty();
			return true;
		}
	};
}

TEST(EditorPluginDirtySave, PlainImplementerDefaultsCleanAndSaveSucceeds)
{
	PlainPlugin plugin;
	IEditorPlugin* p = &plugin;
	EXPECT_FALSE(p->IsDirty());
	EXPECT_TRUE(p->Save());
}

TEST(EditorPluginDirtySave, BasePluginStartsClean)
{
	DirtyTestPlugin plugin;
	IEditorPlugin* p = &plugin;
	EXPECT_FALSE(p->IsDirty());
	EXPECT_TRUE(p->Save());
}

TEST(EditorPluginDirtySave, MarkDirtyVisibleThroughInterface)
{
	DirtyTestPlugin plugin;
	IEditorPlugin* p = &plugin;
	plugin.TestMarkDirty();
	EXPECT_TRUE(p->IsDirty());
}

TEST(EditorPluginDirtySave, DefaultSaveFailsWhileDirty)
{
	DirtyTestPlugin plugin;
	IEditorPlugin* p = &plugin;
	plugin.TestMarkDirty();
	EXPECT_FALSE(p->Save());
	EXPECT_TRUE(p->IsDirty());
}

TEST(EditorPluginDirtySave, ClearDirtyResets)
{
	DirtyTestPlugin plugin;
	IEditorPlugin* p = &plugin;
	plugin.TestMarkDirty();
	plugin.TestClearDirty();
	EXPECT_FALSE(p->IsDirty());
	EXPECT_TRUE(p->Save());
}

TEST(EditorPluginDirtySave, OverriddenSaveClearsDirty)
{
	SavingPlugin plugin;
	IEditorPlugin* p = &plugin;
	plugin.TestMarkDirty();
	ASSERT_TRUE(p->IsDirty());
	EXPECT_TRUE(p->Save());
	EXPECT_FALSE(p->IsDirty());
}

TEST(EditorPluginDirtySave, OnUnloadResetsDirty)
{
	DirtyTestPlugin plugin;
	IEditorPlugin* p = &plugin;
	plugin.TestMarkDirty();
	p->OnUnload();
	EXPECT_FALSE(p->IsDirty());
}
