////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Resources/CellFieldGeometryCache.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <stdint.h>
#include <unordered_map>

namespace Dia
{
    namespace Bgfx
    {
        class ShaderProgram;

        // Opaque GPU value-texture id. Meaning is owned by the factory that produced it.
        using CellFieldGpuTextureHandle = uint32_t;

        // Seam between drain bookkeeping (pure, testable without a GPU) and the texture upload.
        // Texture format is always R32F, point-sampled, clamped; texel (x, y) = cell y*width+x.
        class ICellFieldGpuTextureFactory
        {
        public:
            virtual ~ICellFieldGpuTextureFactory() = default;

            virtual bool Create(uint16_t width, uint16_t height, CellFieldGpuTextureHandle& outHandle) = 0;
            // Full-texture update; count == width*height floats. `values` is only valid for the call.
            virtual void Update(CellFieldGpuTextureHandle handle, const float* values, uint32_t count) = 0;
            virtual void Destroy(CellFieldGpuTextureHandle handle) = 0;
        };

        // Real bgfx implementation (render thread only). The handle is the bgfx texture idx.
        // Remembers each texture's dimensions so Update can address the whole texture.
        class BgfxCellFieldGpuTextureFactory : public ICellFieldGpuTextureFactory
        {
        public:
            bool Create(uint16_t width, uint16_t height, CellFieldGpuTextureHandle& outHandle) override;
            void Update(CellFieldGpuTextureHandle handle, const float* values, uint32_t count) override;
            void Destroy(CellFieldGpuTextureHandle handle) override;

        private:
            struct Dim { uint16_t w = 0; uint16_t h = 0; };
            std::unordered_map<CellFieldGpuTextureHandle, Dim> mDims;
        };

        // Renders CellField layers. Render thread only.
        //
        // Stats source of truth: CellFieldRegistry::GetStats() counts uploads / uploadedBytes /
        // geometryBuilds at Drain. The renderer does not count them again.
        //
        // Draw with a null ShaderProgram still drains the registry and resolves handles (counting
        // stale ones) but skips all bgfx submission, so it is safe with no GPU context.
        class CellFieldRenderer
        {
        public:
            // Production: uses real bgfx geometry/texture factories.
            CellFieldRenderer(unsigned short viewId, ShaderProgram* program, Dia::Graphics::CellFieldRegistry* registry);
            // Injected factories (tests). Factories are borrowed and must outlive the renderer.
            CellFieldRenderer(unsigned short viewId, ShaderProgram* program, Dia::Graphics::CellFieldRegistry* registry,
                              ICellFieldGpuGeometryFactory& geometryFactory, ICellFieldGpuTextureFactory& textureFactory);
            ~CellFieldRenderer();

            void SetCamera(const Dia::Graphics::Camera2D& camera);
            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);

            // Processes queued Create/Update/Destroy commands. Called before drawing each frame.
            void DrainRegistry();
            // Drains the registry, then draws requests in order.
            void Draw(const Dia::Graphics::CellFieldFrameData& frame);
            // Frees every GPU object. Idempotent.
            void Shutdown();

        private:
            class Sink;
            struct Layer
            {
                uint16_t                          generation = 0;
                uint16_t                          width = 0;
                uint16_t                          height = 0;
                Dia::Graphics::CellFieldValueKind kind = Dia::Graphics::CellFieldValueKind::Float;
                CellFieldGeometryKey              geometryKey;
                CellFieldGpuHandle                geometry = 0;
                CellFieldGpuTextureHandle         valueTexture = 0;
            };

            void SubmitLayer(const Dia::Core::CellFieldDrawRequest& request, const Layer& layer);
            void ReleaseLayer(Layer& layer);

            unsigned short                    mViewId;
            ShaderProgram*                    mProgram;   // not owned
            Dia::Graphics::CellFieldRegistry* mRegistry;  // not owned
            Dia::Maths::Vector2D              mCanvasSize;
            Dia::Graphics::Camera2D           mCamera;

            BgfxCellFieldGpuGeometryFactory      mOwnedGeometryFactory;
            BgfxCellFieldGpuTextureFactory       mOwnedTextureFactory;
            ICellFieldGpuTextureFactory&         mTextureFactory;
            Dia::Graphics::HexFlatTopOddQBuilder mHexBuilder;
            CellFieldGeometryCache               mGeometryCache;
            std::unordered_map<uint16_t, Layer>  mLayers;  // by handle index

            CellFieldRenderer(const CellFieldRenderer&) = delete;
            CellFieldRenderer& operator=(const CellFieldRenderer&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
