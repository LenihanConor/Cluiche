#include <gtest/gtest.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Editor;
using namespace Dia::Core;

namespace
{
	// Build a minimal descriptor with a simple echo handler
	EditorActionDescriptor MakeDescriptor(
		const char* name,
		const char* owner = "TestOwner",
		DispatchThread thread = DispatchThread::kMainThread)
	{
		EditorActionDescriptor d;
		d.name           = StringCRC(name);
		d.description    = "test description";
		d.category       = "test";
		d.owner          = owner;
		d.dispatchThread = thread;
		d.handler        = [](const Json::Value& /*p*/) {
			Json::Value r;
			r["success"] = true;
			return r;
		};
		return d;
	}
}

// ---------------------------------------------------------------------------
// Register
// ---------------------------------------------------------------------------

TEST(EditorActionRegistry, RegisterAction_ReturnsTrue)
{
	EditorActionRegistry reg;
	reg.Initialize();

	EXPECT_TRUE(reg.RegisterAction(MakeDescriptor("editor.test_action")));

	reg.Shutdown();
}

TEST(EditorActionRegistry, RegisterAction_EmptyName_ReturnsFalse)
{
	EditorActionRegistry reg;
	reg.Initialize();

	EditorActionDescriptor d = MakeDescriptor("placeholder");
	d.name = StringCRC();   // deliberately empty
	EXPECT_FALSE(reg.RegisterAction(d));

	reg.Shutdown();
}

TEST(EditorActionRegistry, RegisterAction_DuplicateName_ReturnsFalse)
{
	EditorActionRegistry reg;
	reg.Initialize();

	reg.RegisterAction(MakeDescriptor("editor.dup"));
	EXPECT_FALSE(reg.RegisterAction(MakeDescriptor("editor.dup")));

	reg.Shutdown();
}

TEST(EditorActionRegistry, GetManifest_ReflectsRegistrations)
{
	EditorActionRegistry reg;
	reg.Initialize();

	reg.RegisterAction(MakeDescriptor("editor.alpha"));
	reg.RegisterAction(MakeDescriptor("editor.beta"));

	EXPECT_EQ(reg.GetManifest().GetCount(), 2u);

	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// Deregister
// ---------------------------------------------------------------------------

TEST(EditorActionRegistry, DeregisterActionsForOwner_RemovesMatchingEntries)
{
	EditorActionRegistry reg;
	reg.Initialize();

	reg.RegisterAction(MakeDescriptor("editor.owned_a", "OwnerA"));
	reg.RegisterAction(MakeDescriptor("editor.owned_b", "OwnerA"));
	reg.RegisterAction(MakeDescriptor("editor.other",   "OwnerB"));

	reg.DeregisterActionsForOwner(StringCRC("OwnerA"));

	EXPECT_EQ(reg.GetManifest().GetCount(), 1u);
	EXPECT_NE(reg.GetManifest().FindByName(StringCRC("editor.other")), nullptr);

	reg.Shutdown();
}

TEST(EditorActionRegistry, DeregisterActionsForOwner_AfterRemoval_ExecuteReturnsUnknown)
{
	EditorActionRegistry reg;
	reg.Initialize();

	reg.RegisterAction(MakeDescriptor("editor.removable", "TempOwner"));
	reg.DeregisterActionsForOwner(StringCRC("TempOwner"));

	Json::Value result = reg.ExecuteAction(StringCRC("editor.removable"), Json::Value{});

	EXPECT_FALSE(result["success"].asBool());
	EXPECT_STREQ(result["reason"].asCString(), "unknown_action");

	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

TEST(EditorActionRegistry, ExecuteAction_KnownAction_CallsHandler)
{
	EditorActionRegistry reg;
	reg.Initialize();

	reg.RegisterAction(MakeDescriptor("editor.exec_me"));

	Json::Value result = reg.ExecuteAction(StringCRC("editor.exec_me"), Json::Value{});

	EXPECT_TRUE(result["success"].asBool());

	reg.Shutdown();
}

TEST(EditorActionRegistry, ExecuteAction_UnknownName_ReturnsUnknownAction)
{
	EditorActionRegistry reg;
	reg.Initialize();

	Json::Value result = reg.ExecuteAction(StringCRC("editor.does_not_exist"), Json::Value{});

	EXPECT_FALSE(result["success"].asBool());
	EXPECT_STREQ(result["reason"].asCString(), "unknown_action");

	reg.Shutdown();
}

TEST(EditorActionRegistry, ExecuteAction_ParamsPassedToHandler)
{
	EditorActionRegistry reg;
	reg.Initialize();

	EditorActionDescriptor d = MakeDescriptor("editor.echo");
	d.handler = [](const Json::Value& p) { return p; };
	reg.RegisterAction(d);

	Json::Value params;
	params["x"] = 42;
	Json::Value result = reg.ExecuteAction(StringCRC("editor.echo"), params);

	EXPECT_EQ(result["x"].asInt(), 42);

	reg.Shutdown();
}

// ---------------------------------------------------------------------------
// Manifest content
// ---------------------------------------------------------------------------

TEST(EditorActionRegistry, Manifest_ContainsCorrectMetadata)
{
	EditorActionRegistry reg;
	reg.Initialize();

	EditorActionDescriptor d = MakeDescriptor("editor.meta_check", "MetaOwner");
	d.description    = "A rich description for AI tools.";
	d.category       = "editor";
	d.dispatchThread = DispatchThread::kMainThread;
	reg.RegisterAction(d);

	const EditorActionEntry* entry = reg.GetManifest().FindByName(StringCRC("editor.meta_check"));
	ASSERT_NE(entry, nullptr);
	EXPECT_STREQ(entry->description, "A rich description for AI tools.");
	EXPECT_STREQ(entry->category, "editor");
	EXPECT_STREQ(entry->owner, "MetaOwner");
	EXPECT_EQ(entry->dispatchThread, DispatchThread::kMainThread);

	reg.Shutdown();
}
