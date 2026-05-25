////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxTextureHandle.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Resources/BgfxTextureHandle.h"

#include <DiaObservation/Log/DiaLog.h>

#include <bgfx/bgfx.h>

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
                                                  unsigned int height)
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
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
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

        void BgfxTextureHandle::MarkFailed(const char* reason)
        {
            DIA_LOG_ERROR("DiaBgfx", "BgfxTextureHandle: asset 0x%08X failed — %s",
                          mAssetId.Value(), reason ? reason : "unknown");
            mState.store(State::Failed, std::memory_order_release);
        }

    } // namespace Bgfx
} // namespace Dia
