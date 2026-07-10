////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxTextureHandle.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Resources/BgfxTextureHandle.h"

#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>

#include <stb_image.h>

namespace Dia
{
    namespace Bgfx
    {
        BgfxTextureHandle::BgfxTextureHandle(Dia::Core::StringCRC assetId)
            : mAssetId(assetId)
            , mSize(0.0f, 0.0f)
            , mBgfxHandleIdx(bgfx::kInvalidHandle)
            , mState(State::Pending)
        {}

        BgfxTextureHandle::~BgfxTextureHandle()
        {
            if (mBgfxHandleIdx != bgfx::kInvalidHandle)
            {
                bgfx::destroy(bgfx::TextureHandle{ mBgfxHandleIdx });
                mBgfxHandleIdx = bgfx::kInvalidHandle;
            }
        }

        Dia::Maths::Vector2D BgfxTextureHandle::GetSize() const
        {
            return mSize;
        }

        bool BgfxTextureHandle::UploadFromMemory(const unsigned char* rgbaPixels,
                                                  unsigned int width,
                                                  unsigned int height,
                                                  uint64_t bgfxFlags)
        {
            DIA_ASSERT(rgbaPixels != nullptr, "BgfxTextureHandle::UploadFromMemory: null pixels");
            DIA_ASSERT(width > 0 && height > 0, "BgfxTextureHandle::UploadFromMemory: zero dimensions");

            const uint32_t dataSize = width * height * 4u;
            const bgfx::Memory* mem = bgfx::copy(rgbaPixels, dataSize);

            bgfx::TextureHandle handle = bgfx::createTexture2D(
                static_cast<uint16_t>(width),
                static_cast<uint16_t>(height),
                false,   // hasMips
                1,       // numLayers
                bgfx::TextureFormat::RGBA8,
                bgfxFlags | BGFX_SAMPLER_NONE,
                mem
            );

            if (!bgfx::isValid(handle))
            {
                DIA_LOG_ERROR("DiaBgfx", "BgfxTextureHandle: bgfx::createTexture2D failed for asset 0x%08X",
                              mAssetId.Value());
                mState.store(State::Failed, std::memory_order_release);
                return false;
            }

            mBgfxHandleIdx = handle.idx;
            mSize = Dia::Maths::Vector2D(static_cast<float>(width), static_cast<float>(height));
            mState.store(State::Ready, std::memory_order_release);
            return true;
        }

        bool BgfxTextureHandle::UploadFromEncodedMemory(const unsigned char* fileBytes,
                                                         unsigned int byteCount,
                                                         uint64_t bgfxFlags,
                                                         const char** outFailureReason)
        {
            DIA_ASSERT(fileBytes != nullptr && byteCount > 0, "UploadFromEncodedMemory: null/empty input");

            int w = 0, h = 0, channels = 0;
            unsigned char* pixels = stbi_load_from_memory(
                fileBytes,
                static_cast<int>(byteCount),
                &w, &h, &channels,
                4);  // force RGBA

            if (pixels == nullptr)
            {
                if (outFailureReason) *outFailureReason = "stb_image: failed to decode image";
                mState.store(State::Failed, std::memory_order_release);
                return false;
            }

            bool ok = UploadFromMemory(pixels, static_cast<unsigned int>(w), static_cast<unsigned int>(h), bgfxFlags);
            stbi_image_free(pixels);

            if (!ok && outFailureReason)
                *outFailureReason = "bgfx texture upload failed";

            return ok;
        }

        void BgfxTextureHandle::MarkFailed(const char* reason)
        {
            DIA_LOG_ERROR("DiaBgfx", "BgfxTextureHandle: asset 0x%08X failed — %s",
                          mAssetId.Value(), reason ? reason : "unknown");
            mState.store(State::Failed, std::memory_order_release);
        }

    } // namespace Bgfx
} // namespace Dia
