////////////////////////////////////////////////////////////////////////////////
// Filename: FrameCaptureRingBuffer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/FrameCapture.h>
#include <atomic>
#include <cstring>

namespace Dia { namespace Bgfx {

class FrameCaptureRingBuffer
{
public:
    static constexpr unsigned int kDepth = 4;

    FrameCaptureRingBuffer();
    ~FrameCaptureRingBuffer();

    // Claim next free slot. Returns token with id==0 if no slot free.
    Dia::Graphics::FrameCaptureToken Claim();

    // Called from bgfx screenShot callback (render thread).
    // slotIndex is decoded from filePath. Performs BGRA->RGBA swap during copy.
    void OnScreenShot(unsigned int slotIndex,
                      unsigned int width, unsigned int height, unsigned int pitch,
                      const void* data, unsigned int dataSize, bool yflip);

    // Poll result for token. Returns kPending, kReady, kFailed, or kInvalidToken.
    Dia::Graphics::FrameCaptureResult Poll(const Dia::Graphics::FrameCaptureToken& token);

    // Release a ready slot back to free (call after consuming the data).
    void Release(const Dia::Graphics::FrameCaptureToken& token);

    // Build the filePath string for bgfx::requestScreenShot from a slot index.
    static void BuildSlotPath(unsigned int slotIndex, char* outBuf, unsigned int bufSize);

    // Decode slot index from a filePath string. Returns kDepth if not a capture path.
    static unsigned int DecodeSlotIndex(const char* filePath);

    // Decode slot index from a packed token id.
    static unsigned int SlotFromId(unsigned int id);

private:
    enum class SlotState : unsigned char { kFree, kPending, kReady };

    struct RingSlot
    {
        std::atomic<SlotState> state;
        unsigned int           generation;  // Increments each reuse
        unsigned int           width;
        unsigned int           height;
        unsigned int           pitch;
        unsigned char*         pixelData;   // heap-allocated RGBA8
        unsigned int           pixelDataSize;

        RingSlot() : state(SlotState::kFree), generation(0),
                     width(0), height(0), pitch(0),
                     pixelData(nullptr), pixelDataSize(0) {}
    };

    RingSlot     mSlots[kDepth];
    unsigned int mNextSlot;  // Round-robin allocation hint

    static unsigned int PackId(unsigned int slotIndex, unsigned int generation);
    static unsigned int GenFromId(unsigned int id);
};

}} // namespace Dia::Bgfx
