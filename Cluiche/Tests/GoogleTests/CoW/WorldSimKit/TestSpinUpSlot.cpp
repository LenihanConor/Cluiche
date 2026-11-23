#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <Shared/WorldStep/SpinUpSlot.h>

namespace CoW {
namespace WorldStep {
namespace Testing {

namespace {
struct FakeSim { int v; };
using Slot = SpinUpSlot<FakeSim>;
}

TEST(SpinUpSlotTest, StartsIdle)
{
    Slot s;
    EXPECT_EQ(Slot::kIdle, s.State());
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));
}

TEST(SpinUpSlotTest, IdleRunningDoneTakeExactlyOnce)
{
    Slot s;
    FakeSim sim{7};
    EXPECT_TRUE(s.TryBegin(3));
    EXPECT_EQ(Slot::kRunning, s.State());
    EXPECT_EQ(3u, s.Revision());
    EXPECT_TRUE(s.Complete(&sim));
    EXPECT_EQ(Slot::kDone, s.State());

    FakeSim* out = nullptr;
    uint32_t rev = 0;
    ASSERT_TRUE(s.TryTake(out, rev));
    EXPECT_EQ(&sim, out);
    EXPECT_EQ(3u, rev);
    EXPECT_EQ(Slot::kIdle, s.State());
    EXPECT_FALSE(s.TryTake(out, rev));
}

TEST(SpinUpSlotTest, TryTakeFalseWhileRunning)
{
    Slot s;
    s.TryBegin(1);
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));
}

TEST(SpinUpSlotTest, CancelRejectsCompletionAndNothingToTake)
{
    Slot s;
    FakeSim sim{1};
    s.TryBegin(1);
    EXPECT_FALSE(s.IsCancelled());
    s.Cancel();
    EXPECT_TRUE(s.IsCancelled());
    EXPECT_FALSE(s.Complete(&sim));
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));
}

TEST(SpinUpSlotTest, CancelWhileIdleIsNoOp)
{
    Slot s;
    s.Cancel();
    EXPECT_EQ(Slot::kIdle, s.State());
    EXPECT_FALSE(s.IsCancelled());
}

TEST(SpinUpSlotTest, NewRevisionWhileRunningCancelsOldAndBeginsNew)
{
    Slot s;
    FakeSim oldSim{1};
    FakeSim newSim{2};
    ASSERT_TRUE(s.TryBegin(1));
    EXPECT_TRUE(s.TryBegin(2));
    EXPECT_EQ(Slot::kRunning, s.State());
    EXPECT_EQ(2u, s.Revision());
    EXPECT_TRUE(s.IsCancelled(1));
    EXPECT_FALSE(s.IsCancelled(2));

    // Late completion from the old job is rejected; caller keeps ownership.
    EXPECT_FALSE(s.Complete(&oldSim, 1));
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));

    EXPECT_TRUE(s.Complete(&newSim, 2));
    ASSERT_TRUE(s.TryTake(out, rev));
    EXPECT_EQ(&newSim, out);
    EXPECT_EQ(2u, rev);
}

TEST(SpinUpSlotTest, TryBeginAfterCancelledAndAfterTake)
{
    Slot s;
    s.TryBegin(1);
    s.Cancel();
    EXPECT_TRUE(s.TryBegin(2));
    EXPECT_EQ(Slot::kRunning, s.State());
    EXPECT_FALSE(s.IsCancelled());
}

TEST(SpinUpSlotTest, TryBeginRefusedWhileResultUnconsumed)
{
    Slot s;
    FakeSim sim{1};
    s.TryBegin(1);
    s.Complete(&sim);
    EXPECT_FALSE(s.TryBegin(2));
    EXPECT_EQ(Slot::kDone, s.State());
}

TEST(SpinUpSlotTest, ResetReturnsToIdle)
{
    Slot s;
    FakeSim sim{1};
    s.TryBegin(1);
    s.Complete(&sim);
    s.Reset();
    EXPECT_EQ(Slot::kIdle, s.State());
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));
}

TEST(SpinUpSlotTest, WorkerThreadExitsWhenCancelled)
{
    Slot s;
    s.TryBegin(5);
    std::atomic<bool> started(false);
    std::atomic<bool> sawCancel(false);
    FakeSim sim{9};
    bool accepted = true;

    std::thread worker([&]() {
        started.store(true);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!s.IsCancelled())
        {
            if (std::chrono::steady_clock::now() > deadline) { return; }
            std::this_thread::yield();
        }
        sawCancel.store(true);
        accepted = s.Complete(&sim);
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!started.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    s.Cancel();
    worker.join();

    EXPECT_TRUE(sawCancel.load());
    EXPECT_FALSE(accepted);
    FakeSim* out = nullptr;
    uint32_t rev = 0;
    EXPECT_FALSE(s.TryTake(out, rev));
}

}  // namespace Testing
}  // namespace WorldStep
}  // namespace CoW
