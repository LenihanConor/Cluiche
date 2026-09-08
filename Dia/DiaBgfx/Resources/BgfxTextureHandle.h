////////////////////////////////////////////////////////////////////////////////
// Filename: BgfxTextureHandle.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Assets/ITexture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <atomic>

namespace Dia
{
    namespace Bgfx
    {
        // ITexture implementer backed by a bgfx::TextureHandle.
        // bgfx::TextureHandle must NOT appear in any DiaGraphics public surface (RB-006).
        class BgfxTextureHandle : public Dia::Graphics::ITexture
        {
        public:
            explicit BgfxTextureHandle(Dia::Core::StringCRC assetId);
            ~BgfxTextureHandle() override;

            // ITexture
            Dia::Core::StringCRC   GetAssetId() const override { return mAssetId; }
            Dia::Maths::Vector2D   GetSize()    const override;
            State                  GetState()   const override { return mState.load(std::memory_order_acquire); }

            // Called from TextureHandler::Tick on the render thread after image decode.
            // bgfxFlags: BGFX_TEXTURE_* flags (e.g. BGFX_TEXTURE_SRGB for albedo; 0 for linear).
            bool UploadFromMemory(const unsigned char* rgbaPixels, unsigned int width, unsigned int height,
                                  uint64_t bgfxFlags = 0);

            // Decodes encoded image file bytes (PNG/JPG/etc.) via stb_image and uploads to bgfx.
            // bgfxFlags: forwarded to UploadFromMemory (e.g. BGFX_TEXTURE_SRGB for albedo textures).
            // Returns false on decode failure; outFailureReason receives a static string.
            bool UploadFromEncodedMemory(const unsigned char* fileBytes, unsigned int byteCount,
                                         uint64_t bgfxFlags = 0,
                                         const char** outFailureReason = nullptr);

            void MarkFailed(const char* reason);

            // Renderer-internal accessor — DiaBgfx translation units only.
            // Returns bgfx::TextureHandle::idx (uint16_t); caller reconstructs the handle.
            unsigned short GetBgfxHandleIdx() const { return mBgfxHandleIdx; }

        private:
            Dia::Core::StringCRC       mAssetId;
            Dia::Maths::Vector2D       mSize;
            unsigned short             mBgfxHandleIdx;  // bgfx::kInvalidHandle until UploadFromMemory
            std::atomic<State>         mState;

            BgfxTextureHandle(const BgfxTextureHandle&) = delete;
            BgfxTextureHandle& operator=(const BgfxTextureHandle&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
