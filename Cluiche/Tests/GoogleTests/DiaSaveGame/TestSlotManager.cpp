#include <gtest/gtest.h>

#include <DiaSaveGame/SlotManager.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SlotInfo.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdio>
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
        // EnumerateSlots strips ".sav" so id = filename-without-extension
        const char* id = slots.At(i).id.AsChar();
        if (strcmp(id, "slot_enum_a") == 0) foundA = true;
        if (strcmp(id, "slot_enum_b") == 0) foundB = true;
    }

    EXPECT_TRUE(foundA) << "slot_enum_a not found in enumeration";
    EXPECT_TRUE(foundB) << "slot_enum_b not found in enumeration";

    remove("temp/sm_tests/slot_enum_a.sav");
    remove("temp/sm_tests/slot_enum_b.sav");
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
