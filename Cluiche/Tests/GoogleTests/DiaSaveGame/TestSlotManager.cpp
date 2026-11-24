#include <gtest/gtest.h>

#include <DiaSaveGame/SlotManager.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SlotInfo.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdio>
#include <string>
#include <direct.h>

using namespace Dia::SaveGame;
using Dia::Core::StringCRC;

static void EnsureSmTestDir()
{
    _mkdir("temp");
    _mkdir("temp/sm_tests");
}

static SaveConfig MakeSmConfig(uint32_t maxSlots = 0)
{
    return SaveConfig{ "temp/sm_tests/", "slot_{id}.sav", maxSlots, SaveFormat::Json };
}

static void WriteFile(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (f)
    {
        fwrite("x", 1, 1, f);
        fclose(f);
    }
}

// ---------------------------------------------------------------------------
// SlotName_GeneratedFromPattern
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, SlotName_GeneratedFromPattern)
{
    EnsureSmTestDir();

    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);

    char buf[256];
    sm.BuildPath(StringCRC("abc"), buf, sizeof(buf));

    EXPECT_STREQ("temp/sm_tests/slot_abc.sav", buf);
}

// ---------------------------------------------------------------------------
// Delete_RemovesFile
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, Delete_RemovesFile)
{
    EnsureSmTestDir();

    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);

    const char* path = "temp/sm_tests/slot_del.sav";
    WriteFile(path);

    ASSERT_TRUE(sm.SlotExists(StringCRC("del"))) << "File should exist before delete";

    sm.DeleteSlot(StringCRC("del"));

    EXPECT_FALSE(sm.SlotExists(StringCRC("del"))) << "File should be gone after delete";
}

// ---------------------------------------------------------------------------
// Enumerate_ReturnsOnDiskSlots
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, Enumerate_ReturnsOnDiskSlots)
{
    EnsureSmTestDir();

    // Remove any leftover files from previous runs
    remove("temp/sm_tests/slot_enum_a.sav");
    remove("temp/sm_tests/slot_enum_b.sav");

    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);

    WriteFile("temp/sm_tests/slot_enum_a.sav");
    WriteFile("temp/sm_tests/slot_enum_b.sav");

    Dia::Core::Containers::DynamicArrayC<SlotInfo, 32> slots;
    sm.EnumerateSlots(slots);

    // We need at least 2 slots; there may be others from other tests
    bool foundA = false;
    bool foundB = false;
    for (uint32_t i = 0; i < slots.Size(); ++i)
    {
        // EnumerateSlots returns the id that was passed to Save: the filename minus the
        // slotPattern prefix ("slot_") and suffix (".sav").
        const char* id = slots.At(i).id.AsChar();
        if (strcmp(id, "enum_a") == 0) foundA = true;
        if (strcmp(id, "enum_b") == 0) foundB = true;
    }

    EXPECT_TRUE(foundA) << "enum_a not found in enumeration";
    EXPECT_TRUE(foundB) << "enum_b not found in enumeration";

    // The enumerated id must round-trip into BuildPath/SlotExists.
    EXPECT_TRUE(sm.SlotExists(StringCRC("enum_a")));

    remove("temp/sm_tests/slot_enum_a.sav");
    remove("temp/sm_tests/slot_enum_b.sav");
}

// ---------------------------------------------------------------------------
// Rename_MovesFile
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, Rename_MovesFile)
{
    EnsureSmTestDir();

    remove("temp/sm_tests/slot_ren_src.sav");
    remove("temp/sm_tests/slot_ren_dst.sav");

    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);

    WriteFile("temp/sm_tests/slot_ren_src.sav");

    ASSERT_TRUE(sm.SlotExists(StringCRC("ren_src"))) << "Source file should exist before rename";
    ASSERT_FALSE(sm.SlotExists(StringCRC("ren_dst"))) << "Destination should not exist before rename";

    sm.RenameSlot(StringCRC("ren_src"), StringCRC("ren_dst"));

    EXPECT_FALSE(sm.SlotExists(StringCRC("ren_src"))) << "Source should be gone after rename";
    EXPECT_TRUE(sm.SlotExists(StringCRC("ren_dst")))  << "Destination should exist after rename";

    remove("temp/sm_tests/slot_ren_dst.sav");
}

// ---------------------------------------------------------------------------
// MaxSlotCap_RejectsOverflow
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, MaxSlotCap_RejectsOverflow)
{
    EnsureSmTestDir();

    remove("temp/sm_tests/slot_cap_0.sav");
    remove("temp/sm_tests/slot_cap_1.sav");

    // maxSlots = 2
    SaveConfig cfg = MakeSmConfig(2);
    SlotManager sm;
    sm.Init(cfg);

    EXPECT_FALSE(sm.IsMaxSlotsReached()) << "Should not be at capacity before any files";

    WriteFile("temp/sm_tests/slot_cap_0.sav");
    EXPECT_FALSE(sm.IsMaxSlotsReached()) << "Should not be at capacity with 1 of 2 slots";

    WriteFile("temp/sm_tests/slot_cap_1.sav");
    EXPECT_TRUE(sm.IsMaxSlotsReached()) << "Should be at capacity with 2 of 2 slots";

    remove("temp/sm_tests/slot_cap_0.sav");
    remove("temp/sm_tests/slot_cap_1.sav");
}

// ---------------------------------------------------------------------------
// Slot id validation
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_SlotManager, IsValidSlotId_Table)
{
    EXPECT_TRUE(SlotManager::IsValidSlotId("quick"));
    EXPECT_TRUE(SlotManager::IsValidSlotId("slot-1"));
    EXPECT_TRUE(SlotManager::IsValidSlotId("A_b.2"));
    EXPECT_FALSE(SlotManager::IsValidSlotId(nullptr));
    EXPECT_FALSE(SlotManager::IsValidSlotId(""));
    EXPECT_FALSE(SlotManager::IsValidSlotId("../x"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("a/b"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("a\\b"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("C:x"));
    EXPECT_FALSE(SlotManager::IsValidSlotId(".hidden"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("a..b"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("CON"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("nul"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("com1"));
    EXPECT_FALSE(SlotManager::IsValidSlotId("con.txt"));
    EXPECT_TRUE(SlotManager::IsValidSlotId("console"));
    EXPECT_FALSE(SlotManager::IsValidSlotId(std::string(200, 'a').c_str()));
}

TEST(DiaSaveGame_SlotManager, BuildPath_InvalidIdGivesEmptyPath_SlotExistsFalse)
{
    EnsureSmTestDir();
    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);

    char buf[256] = "garbage";
    sm.BuildPath(StringCRC("../x"), buf, sizeof(buf));
    EXPECT_STREQ("", buf);
    EXPECT_FALSE(sm.SlotExists(StringCRC("../x")));
}

TEST(DiaSaveGame_SlotManager, EnumerateIgnoresTmpFiles)
{
    EnsureSmTestDir();
    WriteFile("temp/sm_tests/slot_tmpa.sav");
    WriteFile("temp/sm_tests/slot_tmpa.sav.tmp");

    SaveConfig cfg = MakeSmConfig();
    SlotManager sm;
    sm.Init(cfg);
    Dia::Core::Containers::DynamicArrayC<SlotInfo, 32> out;
    sm.EnumerateSlots(out);
    int count = 0;
    for (uint32_t i = 0; i < out.Size(); ++i)
    {
        if (out[i].id == StringCRC("tmpa")) ++count;
        EXPECT_STRNE("tmpa.sav", out[i].id.AsChar());
    }
    EXPECT_EQ(1, count);

    remove("temp/sm_tests/slot_tmpa.sav");
    remove("temp/sm_tests/slot_tmpa.sav.tmp");
}
