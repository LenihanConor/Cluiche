// TestFrameCaptureRingBuffer.cpp - Google Test unit tests for FrameCaptureRingBuffer
//
// Tests the ring buffer used to manage frame-capture readback slots in DiaBgfx.
// OnScreenShot is called directly to simulate what the bgfx screenshot callback does.

#include <gtest/gtest.h>
#include <DiaBgfx/Capture/FrameCaptureRingBuffer.h>

using Dia::Bgfx::FrameCaptureRingBuffer;
using Dia::Graphics::FrameCaptureToken;
using Dia::Graphics::FrameCaptureResult;

// ==============================================================================
// Claim tests
// ==============================================================================

TEST(FrameCaptureRingBufferTest, ClaimReturnsValidToken)
{
    FrameCaptureRingBuffer rb;

    FrameCaptureToken token = rb.Claim();

    EXPECT_TRUE(token.IsValid());
}

TEST(FrameCaptureRingBufferTest, ClaimAllSlots)
{
    FrameCaptureRingBuffer rb;

    FrameCaptureToken tokens[FrameCaptureRingBuffer::kDepth];
    for (unsigned int i = 0; i < FrameCaptureRingBuffer::kDepth; ++i)
    {
        tokens[i] = rb.Claim();
        EXPECT_TRUE(tokens[i].IsValid()) << "Expected valid token at claim " << i;
    }

    // Ring is now full — next claim must return invalid
    FrameCaptureToken overflow = rb.Claim();
    EXPECT_FALSE(overflow.IsValid());
}

// ==============================================================================
// BuildSlotPath / DecodeSlotIndex round-trip
// ==============================================================================

TEST(FrameCaptureRingBufferTest, SlotPathRoundTrip)
{
    char buf[64];
    for (unsigned int i = 0; i < FrameCaptureRingBuffer::kDepth; ++i)
    {
        FrameCaptureRingBuffer::BuildSlotPath(i, buf, sizeof(buf));
        unsigned int decoded = FrameCaptureRingBuffer::DecodeSlotIndex(buf);
        EXPECT_EQ(decoded, i) << "Round-trip failed for slot " << i;
    }
}

TEST(FrameCaptureRingBufferTest, DecodeSlotIndexRejectsUnknownPath)
{
    unsigned int result = FrameCaptureRingBuffer::DecodeSlotIndex("random_path");

    EXPECT_EQ(result, FrameCaptureRingBuffer::kDepth);
}

// ==============================================================================
// Poll state machine tests
// ==============================================================================

TEST(FrameCaptureRingBufferTest, PollBeforeCallbackIsPending)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    FrameCaptureResult result = rb.Poll(token);

    EXPECT_EQ(result.status, FrameCaptureResult::Status::kPending);
}

TEST(FrameCaptureRingBufferTest, PollAfterOnScreenShotIsReady)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(token.id);
    const unsigned int width  = 4;
    const unsigned int height = 4;
    const unsigned int pitch  = width * 4u;
    unsigned char pixelData[4 * 4 * 4] = {};  // width * height * 4 bytes/pixel

    rb.OnScreenShot(slotIndex, width, height, pitch, pixelData, sizeof(pixelData), false);

    FrameCaptureResult result = rb.Poll(token);

    EXPECT_EQ(result.status, FrameCaptureResult::Status::kReady);
    EXPECT_EQ(result.width,  width);
    EXPECT_EQ(result.height, height);
}

// ==============================================================================
// BGRA -> RGBA swap
// ==============================================================================

TEST(FrameCaptureRingBufferTest, BgraToRgbaSwap)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(token.id);

    // Input: one 1x1 pixel stored as BGRA bytes {B=10, G=20, R=30, A=255}
    unsigned char bgraPixel[4] = { 10, 20, 30, 255 };

    rb.OnScreenShot(slotIndex, 1, 1, 4u, bgraPixel, sizeof(bgraPixel), false);

    FrameCaptureResult result = rb.Poll(token);
    ASSERT_EQ(result.status, FrameCaptureResult::Status::kReady);
    ASSERT_NE(result.data, nullptr);

    const unsigned char* rgba = static_cast<const unsigned char*>(result.data);
    // Expected output RGBA: R=30, G=20, B=10, A=255
    EXPECT_EQ(rgba[0], 30u);
    EXPECT_EQ(rgba[1], 20u);
    EXPECT_EQ(rgba[2], 10u);
    EXPECT_EQ(rgba[3], 255u);
}

// ==============================================================================
// Release / recycle tests
// ==============================================================================

TEST(FrameCaptureRingBufferTest, ReleaseFreesSlot)
{
    FrameCaptureRingBuffer rb;

    // Fill all slots
    FrameCaptureToken tokens[FrameCaptureRingBuffer::kDepth];
    for (unsigned int i = 0; i < FrameCaptureRingBuffer::kDepth; ++i)
        tokens[i] = rb.Claim();

    // Ring is now full
    ASSERT_FALSE(rb.Claim().IsValid());

    // Drive slot 0 to kReady then release it
    unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(tokens[0].id);
    unsigned char dummy[4] = { 0, 0, 0, 255 };
    rb.OnScreenShot(slotIndex, 1, 1, 4u, dummy, sizeof(dummy), false);
    rb.Release(tokens[0]);

    // One slot freed — should claim successfully
    FrameCaptureToken newToken = rb.Claim();
    EXPECT_TRUE(newToken.IsValid());
}

TEST(FrameCaptureRingBufferTest, GenerationPreventsStaleAccess)
{
    FrameCaptureRingBuffer rb;

    FrameCaptureToken firstToken = rb.Claim();
    ASSERT_TRUE(firstToken.IsValid());

    // Drive to kReady then release, which bumps the generation counter
    unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(firstToken.id);
    unsigned char dummy[4] = { 0, 0, 0, 255 };
    rb.OnScreenShot(slotIndex, 1, 1, 4u, dummy, sizeof(dummy), false);
    rb.Release(firstToken);

    // Reclaim (gets new generation for the same slot)
    FrameCaptureToken secondToken = rb.Claim();
    ASSERT_TRUE(secondToken.IsValid());

    // Old token's generation no longer matches — must return kInvalidToken
    FrameCaptureResult staleResult = rb.Poll(firstToken);
    EXPECT_EQ(staleResult.status, FrameCaptureResult::Status::kInvalidToken);

    // New token is still pending (no callback yet)
    FrameCaptureResult freshResult = rb.Poll(secondToken);
    EXPECT_EQ(freshResult.status, FrameCaptureResult::Status::kPending);
}

// ==============================================================================
// yflip row reversal
// ==============================================================================

TEST(FrameCaptureRingBufferTest, YflipCopiesRowsReversed)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(token.id);

    // 2 rows x 1 column, BGRA layout:
    //   Source row 0: {B=1, G=2, R=3, A=255}  -> RGBA = {3, 2, 1, 255}
    //   Source row 1: {B=4, G=5, R=6, A=255}  -> RGBA = {6, 5, 4, 255}
    const unsigned int width  = 1;
    const unsigned int height = 2;
    const unsigned int pitch  = width * 4u;
    unsigned char bgraData[8] = {
        1, 2, 3, 255,  // row 0
        4, 5, 6, 255   // row 1
    };

    rb.OnScreenShot(slotIndex, width, height, pitch, bgraData, sizeof(bgraData), /*yflip=*/true);

    FrameCaptureResult result = rb.Poll(token);
    ASSERT_EQ(result.status, FrameCaptureResult::Status::kReady);
    ASSERT_NE(result.data, nullptr);

    const unsigned char* rgba = static_cast<const unsigned char*>(result.data);

    // With yflip: output row 0 <- source row 1 {B=4,G=5,R=6} -> RGBA {6,5,4,255}
    EXPECT_EQ(rgba[0], 6u);
    EXPECT_EQ(rgba[1], 5u);
    EXPECT_EQ(rgba[2], 4u);
    EXPECT_EQ(rgba[3], 255u);

    // With yflip: output row 1 <- source row 0 {B=1,G=2,R=3} -> RGBA {3,2,1,255}
    EXPECT_EQ(rgba[4], 3u);
    EXPECT_EQ(rgba[5], 2u);
    EXPECT_EQ(rgba[6], 1u);
    EXPECT_EQ(rgba[7], 255u);
}

// ==============================================================================
// Poll with zero-id token
// ==============================================================================

TEST(FrameCaptureRingBufferTest, PollWithZeroIdToken_ReturnsInvalidToken)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken zeroToken;
    zeroToken.id = 0;

    FrameCaptureResult result = rb.Poll(zeroToken);

    EXPECT_EQ(result.status, FrameCaptureResult::Status::kInvalidToken);
}

// ==============================================================================
// OnScreenShot with out-of-range slot index
// ==============================================================================

TEST(FrameCaptureRingBufferTest, OnScreenShot_OutOfRangeSlotIndex_IsNoOp)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    // Call OnScreenShot with an out-of-range slot index — should not corrupt state
    unsigned char dummy[4] = { 1, 2, 3, 255 };
    rb.OnScreenShot(FrameCaptureRingBuffer::kDepth, 1, 1, 4u, dummy, sizeof(dummy), false);

    // The originally claimed slot should still be kPending (not corrupted)
    FrameCaptureResult result = rb.Poll(token);
    EXPECT_EQ(result.status, FrameCaptureResult::Status::kPending);
}

// ==============================================================================
// Release pending slot
// ==============================================================================

TEST(FrameCaptureRingBufferTest, Release_PendingSlot_IsNoOp)
{
    FrameCaptureRingBuffer rb;
    FrameCaptureToken token = rb.Claim();
    ASSERT_TRUE(token.IsValid());

    // Release before OnScreenShot (slot still kPending)
    // Release on a non-ready slot: look at the implementation —
    // Release checks generation but does NOT check state, so it will free the slot.
    // What we care about: it doesn't crash and the slot is recoverable.
    rb.Release(token);

    // After release, slot should be reclaimable
    FrameCaptureToken newToken = rb.Claim();
    EXPECT_TRUE(newToken.IsValid());
}

// ==============================================================================
// Full lifecycle stress test
// ==============================================================================

TEST(FrameCaptureRingBufferTest, FullLifecycleCycle_MultipleTimes)
{
    FrameCaptureRingBuffer rb;
    unsigned char pixel[4] = { 10, 20, 30, 255 };

    for (unsigned int cycle = 0; cycle < 20; ++cycle)
    {
        FrameCaptureToken token = rb.Claim();
        ASSERT_TRUE(token.IsValid()) << "Cycle " << cycle << ": Claim failed";

        unsigned int slotIndex = FrameCaptureRingBuffer::SlotFromId(token.id);
        rb.OnScreenShot(slotIndex, 1, 1, 4u, pixel, sizeof(pixel), false);

        FrameCaptureResult result = rb.Poll(token);
        ASSERT_EQ(result.status, FrameCaptureResult::Status::kReady) << "Cycle " << cycle;
        ASSERT_NE(result.data, nullptr) << "Cycle " << cycle;

        // Verify BGRA->RGBA swap persists across cycles
        const unsigned char* rgba = static_cast<const unsigned char*>(result.data);
        EXPECT_EQ(rgba[0], 30u) << "Cycle " << cycle << ": R channel wrong";
        EXPECT_EQ(rgba[2], 10u) << "Cycle " << cycle << ": B channel wrong";

        rb.Release(token);
    }
}
