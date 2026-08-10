#include <gtest/gtest.h>

#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaSaveGame/SaveResult.h>
#include <DiaSaveGame/Testing/SaveTestHelpers.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdio>
#include <direct.h>

using namespace Dia::SaveGame;
using namespace Dia::SaveGame::Testing;
using Dia::Core::StringCRC;

static void EnsureAsyncTestDir()
{
    _mkdir("temp");
    _mkdir("temp/savegame_async_tests");
}

static SaveConfig MakeAsyncConfig()
{
    return SaveConfig{ "temp/savegame_async_tests/", "slot_{id}.sav", 0, SaveFormat::Json };
}

// ---------------------------------------------------------------------------
// SaveThenLoad_AsyncPath
//   Exercises Save + Load via the threaded I/O path. Verifies that the data
//   written on the sim thread is fully flushed and the subsequent Load
//   correctly deserializes it back on the calling thread.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Async, SaveThenLoad_AsyncPath)
{
    EnsureAsyncTestDir();

    MockSaveable participant;
    participant.valueToWrite = 77;
    participant.version      = 1;

    SaveRegistry reg;
    reg.Register(StringCRC("async_p"), &participant);

    SaveConfig  cfg = MakeAsyncConfig();
    SaveManager mgr;
    mgr.Init(cfg, reg);

    // Save — disk write happens off-thread, but we join before returning
    SaveResult sr = mgr.Save(StringCRC("async_slot"));
    ASSERT_TRUE(sr.Ok()) << "Save failed: " << static_cast<int>(sr.code);

    // Verify the file exists on disk (confirms the thread actually flushed)
    ASSERT_TRUE(AssertSlotExists(cfg, StringCRC("async_slot")))
        << "Save file not found after async save";

    // Load — disk read happens off-thread, parse + deserialize on calling thread
    participant.lastReadValue    = 0;
    participant.deserializeCount = 0;

    LoadResult lr = mgr.Load(StringCRC("async_slot"));
    ASSERT_TRUE(lr.Ok()) << "Load failed: " << static_cast<int>(lr.code);

    EXPECT_EQ(1,  participant.deserializeCount) << "Deserialize should have been called once";
    EXPECT_EQ(77, participant.lastReadValue)    << "Loaded value should match what was saved";

    remove("temp/savegame_async_tests/slot_async_slot.sav");
}

// ---------------------------------------------------------------------------
// MultipleConsecutiveSaves_NoStaticBufferAliasing
//   Saves two different slots consecutively. If Save used a static buffer,
//   the second save could stomp the first before the write thread flushes.
//   With a heap buffer per call, both saves must succeed and round-trip.
// ---------------------------------------------------------------------------

TEST(DiaSaveGame_Async, MultipleConsecutiveSaves_NoStaticBufferAliasing)
{
    EnsureAsyncTestDir();

    MockSaveable p1;
    p1.valueToWrite = 11;
    p1.version      = 1;

    MockSaveable p2;
    p2.valueToWrite = 22;
    p2.version      = 1;

    SaveConfig cfg = MakeAsyncConfig();

    // Save slot A
    {
        SaveRegistry regA;
        regA.Register(StringCRC("pA"), &p1);
        SaveManager mgrA;
        mgrA.Init(cfg, regA);
        ASSERT_TRUE(mgrA.Save(StringCRC("multi_a")).Ok());
    }

    // Save slot B immediately after (no gap — tests buffer independence)
    {
        SaveRegistry regB;
        regB.Register(StringCRC("pB"), &p2);
        SaveManager mgrB;
        mgrB.Init(cfg, regB);
        ASSERT_TRUE(mgrB.Save(StringCRC("multi_b")).Ok());
    }

    // Load and verify both
    {
        p1.lastReadValue = 0;
        SaveRegistry regA;
        regA.Register(StringCRC("pA"), &p1);
        SaveManager mgrA;
        mgrA.Init(cfg, regA);
        ASSERT_TRUE(mgrA.Load(StringCRC("multi_a")).Ok());
        EXPECT_EQ(11, p1.lastReadValue);
    }
    {
        p2.lastReadValue = 0;
        SaveRegistry regB;
        regB.Register(StringCRC("pB"), &p2);
        SaveManager mgrB;
        mgrB.Init(cfg, regB);
        ASSERT_TRUE(mgrB.Load(StringCRC("multi_b")).Ok());
        EXPECT_EQ(22, p2.lastReadValue);
    }

    remove("temp/savegame_async_tests/slot_multi_a.sav");
    remove("temp/savegame_async_tests/slot_multi_b.sav");
}
