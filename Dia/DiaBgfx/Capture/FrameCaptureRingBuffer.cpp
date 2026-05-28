////////////////////////////////////////////////////////////////////////////////
// Filename: FrameCaptureRingBuffer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Capture/FrameCaptureRingBuffer.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <cstdlib>   // malloc, free
#include <cstring>   // memcpy

namespace Dia { namespace Bgfx {

// ---------- PackId / SlotFromId / GenFromId ----------
// id layout: bits [31..8] = generation (24 bits), bits [7..0] = slotIndex+1 (1-based so id != 0 for slot 0)

/*static*/ unsigned int FrameCaptureRingBuffer::PackId(unsigned int slotIndex, unsigned int generation)
{
    return ((generation & 0xFFFFFFu) << 8) | ((slotIndex & 0xFFu) + 1u);
}

/*static*/ unsigned int FrameCaptureRingBuffer::SlotFromId(unsigned int id)
{
    return (id & 0xFFu) - 1u;
}

/*static*/ unsigned int FrameCaptureRingBuffer::GenFromId(unsigned int id)
{
    return (id >> 8) & 0xFFFFFFu;
}

// ---------- BuildSlotPath / DecodeSlotIndex ----------

/*static*/ void FrameCaptureRingBuffer::BuildSlotPath(unsigned int slotIndex, char* outBuf, unsigned int bufSize)
{
    // Produces strings "__dia_capture_0" through "__dia_capture_3"
    const char* prefix = "__dia_capture_";
    unsigned int i = 0;
    while (prefix[i] != '\0' && i + 2 < bufSize)
    {
        outBuf[i] = prefix[i];
        ++i;
    }
    if (i + 2 <= bufSize)
    {
        outBuf[i]     = static_cast<char>('0' + (slotIndex & 0xFu));
        outBuf[i + 1] = '\0';
    }
}

/*static*/ unsigned int FrameCaptureRingBuffer::DecodeSlotIndex(const char* filePath)
{
    if (filePath == nullptr)
        return kDepth;

    const char* prefix = "__dia_capture_";
    unsigned int i = 0;
    while (prefix[i] != '\0')
    {
        if (filePath[i] != prefix[i])
            return kDepth;
        ++i;
    }
    // filePath[i] should be the digit character
    char digitChar = filePath[i];
    if (digitChar < '0' || digitChar > '3')
        return kDepth;
    return static_cast<unsigned int>(digitChar - '0');
}

// ---------- Constructor / Destructor ----------

FrameCaptureRingBuffer::FrameCaptureRingBuffer()
    : mNextSlot(0)
{
    // RingSlot default constructor handles member init
}

FrameCaptureRingBuffer::~FrameCaptureRingBuffer()
{
    for (unsigned int i = 0; i < kDepth; ++i)
    {
        if (mSlots[i].pixelData != nullptr)
        {
            free(mSlots[i].pixelData);
            mSlots[i].pixelData = nullptr;
        }
    }
}

// ---------- Claim ----------

Dia::Graphics::FrameCaptureToken FrameCaptureRingBuffer::Claim()
{
    for (unsigned int attempt = 0; attempt < kDepth; ++attempt)
    {
        unsigned int index = (mNextSlot + attempt) % kDepth;
        RingSlot& slot = mSlots[index];

        SlotState expected = SlotState::kFree;
        if (slot.state.compare_exchange_strong(expected, SlotState::kPending))
        {
            mNextSlot = (index + 1) % kDepth;
            Dia::Graphics::FrameCaptureToken token;
            token.id = PackId(index, slot.generation);
            return token;
        }
    }

    // No free slot found
    Dia::Graphics::FrameCaptureToken invalid;
    invalid.id = 0;
    return invalid;
}

// ---------- OnScreenShot ----------

void FrameCaptureRingBuffer::OnScreenShot(unsigned int slotIndex,
                                           unsigned int width, unsigned int height, unsigned int pitch,
                                           const void* data, unsigned int /*dataSize*/, bool yflip)
{
    if (slotIndex >= kDepth)
    {
        DIA_LOG_ERROR("DiaBgfx", "FrameCaptureRingBuffer::OnScreenShot — slotIndex %u out of range", slotIndex);
        return;
    }

    RingSlot& slot = mSlots[slotIndex];

    SlotState currentState = slot.state.load();
    if (currentState != SlotState::kPending)
    {
        DIA_LOG_WARNING("DiaBgfx", "FrameCaptureRingBuffer::OnScreenShot — slot %u not in kPending state", slotIndex);
        return;
    }

    if (data == nullptr)
    {
        DIA_LOG_ERROR("DiaBgfx", "FrameCaptureRingBuffer::OnScreenShot — null data for slot %u", slotIndex);
        slot.state.store(SlotState::kFree);
        return;
    }

    // RGBA8 output: always 4 bytes per pixel
    const unsigned int rgbaSize = width * height * 4u;

    // Reallocate pixel buffer if needed
    if (slot.pixelData == nullptr || slot.pixelDataSize < rgbaSize)
    {
        if (slot.pixelData != nullptr)
        {
            free(slot.pixelData);
            slot.pixelData = nullptr;
        }
        slot.pixelData = static_cast<unsigned char*>(malloc(rgbaSize));
        if (slot.pixelData == nullptr)
        {
            DIA_LOG_ERROR("DiaBgfx", "FrameCaptureRingBuffer::OnScreenShot — malloc failed for slot %u (%u bytes)", slotIndex, rgbaSize);
            slot.state.store(SlotState::kFree);
            return;
        }
        slot.pixelDataSize = rgbaSize;
    }

    // Copy with BGRA->RGBA swap, optionally reversing row order for yflip
    const unsigned char* src = static_cast<const unsigned char*>(data);
    unsigned char* dst = slot.pixelData;

    DIA_PROFILE_SCOPE("capture.bgra_to_rgba", ::Dia::Observation::Profile::Category::kDiaGraphics);
    for (unsigned int row = 0; row < height; ++row)
    {
        unsigned int srcRow = yflip ? (height - 1u - row) : row;
        const unsigned char* srcLine = src + srcRow * pitch;
        unsigned char* dstLine = dst + row * (width * 4u);

        for (unsigned int col = 0; col < width; ++col)
        {
            const unsigned char* srcPixel = srcLine + col * 4u;
            unsigned char* dstPixel = dstLine + col * 4u;
            dstPixel[0] = srcPixel[2]; // R <- B
            dstPixel[1] = srcPixel[1]; // G <- G
            dstPixel[2] = srcPixel[0]; // B <- R
            dstPixel[3] = srcPixel[3]; // A <- A
        }
    }

    slot.width  = width;
    slot.height = height;
    slot.pitch  = width * 4u; // RGBA pitch (always tightly packed)

    slot.state.store(SlotState::kReady);
}

// ---------- Poll ----------

Dia::Graphics::FrameCaptureResult FrameCaptureRingBuffer::Poll(const Dia::Graphics::FrameCaptureToken& token)
{
    Dia::Graphics::FrameCaptureResult result;

    if (token.id == 0)
    {
        result.status = Dia::Graphics::FrameCaptureResult::Status::kInvalidToken;
        return result;
    }

    unsigned int slotIndex = SlotFromId(token.id);
    unsigned int gen       = GenFromId(token.id);

    if (slotIndex >= kDepth)
    {
        result.status = Dia::Graphics::FrameCaptureResult::Status::kInvalidToken;
        return result;
    }

    RingSlot& slot = mSlots[slotIndex];

    if (slot.generation != gen)
    {
        result.status = Dia::Graphics::FrameCaptureResult::Status::kInvalidToken;
        return result;
    }

    SlotState state = slot.state.load();

    switch (state)
    {
        case SlotState::kFree:
            result.status = Dia::Graphics::FrameCaptureResult::Status::kInvalidToken;
            break;

        case SlotState::kPending:
            result.status = Dia::Graphics::FrameCaptureResult::Status::kPending;
            break;

        case SlotState::kReady:
            result.status = Dia::Graphics::FrameCaptureResult::Status::kReady;
            result.data   = slot.pixelData;
            result.width  = slot.width;
            result.height = slot.height;
            result.pitch  = slot.pitch;
            break;
    }

    return result;
}

// ---------- Release ----------

void FrameCaptureRingBuffer::Release(const Dia::Graphics::FrameCaptureToken& token)
{
    if (token.id == 0)
        return;

    unsigned int slotIndex = SlotFromId(token.id);
    unsigned int gen       = GenFromId(token.id);

    if (slotIndex >= kDepth)
        return;

    RingSlot& slot = mSlots[slotIndex];

    if (slot.generation != gen)
        return;

    // Free pixel buffer and recycle slot
    if (slot.pixelData != nullptr)
    {
        free(slot.pixelData);
        slot.pixelData = nullptr;
        slot.pixelDataSize = 0;
    }

    slot.width  = 0;
    slot.height = 0;
    slot.pitch  = 0;
    ++slot.generation;
    slot.state.store(SlotState::kFree);
}

}} // namespace Dia::Bgfx
