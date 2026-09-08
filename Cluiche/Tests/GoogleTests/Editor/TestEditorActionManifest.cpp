#include <gtest/gtest.h>
#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    EditorActionEntry MakeEntry(const char* name, const char* owner = "OwnerA")
    {
        EditorActionEntry entry;
        entry.name  = StringCRC(name);
        entry.owner = owner;
        entry.dispatchThread = DispatchThread::kMainThread;
        return entry;
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(EditorActionManifest, AddAndGetCount)
{
    EditorActionManifest manifest;
    manifest.Add(MakeEntry("project.open_path"));
    manifest.Add(MakeEntry("project.close"));

    EXPECT_EQ(manifest.GetCount(), 2u);
}

TEST(EditorActionManifest, FindByName_Found)
{
    EditorActionManifest manifest;
    manifest.Add(MakeEntry("project.open_path"));

    const EditorActionEntry* result = manifest.FindByName(StringCRC("project.open_path"));

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->name == StringCRC("project.open_path"));
}

TEST(EditorActionManifest, FindByName_NotFound)
{
    EditorActionManifest manifest;
    manifest.Add(MakeEntry("project.open_path"));

    const EditorActionEntry* result = manifest.FindByName(StringCRC("unknown"));

    EXPECT_EQ(result, nullptr);
}

TEST(EditorActionManifest, RemoveByOwner)
{
    EditorActionManifest manifest;
    manifest.Add(MakeEntry("action.a1", "A"));
    manifest.Add(MakeEntry("action.a2", "A"));
    manifest.Add(MakeEntry("action.b1", "B"));

    manifest.RemoveByOwner(StringCRC("A"));

    EXPECT_EQ(manifest.GetCount(), 1u);

    const EditorActionEntry* remaining = manifest.FindByName(StringCRC("action.b1"));
    ASSERT_NE(remaining, nullptr);
    EXPECT_STREQ(remaining->owner, "B");
}

TEST(EditorActionManifest, GetAt)
{
    EditorActionManifest manifest;
    manifest.Add(MakeEntry("plugin.reload"));

    const EditorActionEntry& entry = manifest.GetAt(0);

    EXPECT_TRUE(entry.name == StringCRC("plugin.reload"));
}
