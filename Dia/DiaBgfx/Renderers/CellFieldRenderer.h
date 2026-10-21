////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Renderers/CellFieldDrawParams.h>
#include <DiaBgfx/Renderers/CellFieldViewOrder.h>
#include <DiaBgfx/Resources/CellFieldGeometryCache.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaGraphics/CellField/HexFlatTopOddQBuilder.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Frame/CellFieldFrameData.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

        // Opaque GPU colour-map (palette / ramp) texture id.
        using CellFieldGpuColourMapHandle = uint32_t;

        // Seam for the small colour-map textures. Texels are packed 0xRRGGBBAA; the factory owns
        // the conversion to the GPU byte order. Clamped; point-sampled for palettes, linear for ramps.
        class ICellFieldGpuColourMapFactory
        {
        public:
            virtual ~ICellFieldGpuColourMapFactory() = default;

            virtual bool CreateRgba8(uint32_t width, const uint32_t* texels, uint32_t count, bool linearFilter, CellFieldGpuColourMapHandle& outHandle) = 0;
            virtual void UpdateRgba8(CellFieldGpuColourMapHandle handle, const uint32_t* texels, uint32_t count) = 0;
            virtual void Destroy(CellFieldGpuColourMapHandle handle) = 0;
        };

        // Real bgfx implementation (render thread only). width x 1 RGBA8 textures.
        class BgfxCellFieldGpuColourMapFactory : public ICellFieldGpuColourMapFactory
        {
        public:
            bool CreateRgba8(uint32_t width, const uint32_t* texels, uint32_t count, bool linearFilter, CellFieldGpuColourMapHandle& outHandle) override;
            void UpdateRgba8(CellFieldGpuColourMapHandle handle, const uint32_t* texels, uint32_t count) override;
            void Destroy(CellFieldGpuColourMapHandle handle) override;

        private:
            std::unordered_map<CellFieldGpuColourMapHandle, uint32_t> mWidths;
        };

        // Renderer-side counters (render thread; read via GetRendererStats).
        struct CellFieldRendererStats
        {
            uint64_t unknownColourMaps = 0;       // draws skipped: colour-map id not registered (cellfield.unknown_colour_maps)
            uint64_t outOfRangeCategoryCells = 0; // cells whose id fell outside the palette     (cellfield.out_of_range_cells)
            uint64_t colourMapUploads = 0;        // palette texture creates + revision updates
            uint32_t colourMapsResident = 0;
            uint64_t drawCalls = 0;               // planned submits (layers + backdrops), counted even with no program
            uint64_t backdropDraws = 0;           // of which backdrop rectangles
            uint64_t unbuiltLayerRequests = 0;    // Updates/draws skipped: registry layer is live but its geometry/texture build failed
            uint32_t valueTexturesResident = 0;   // per-layer value textures + the dummy elevation texture currently alive
            uint64_t geometryBytes = 0;           // GPU vertex + index bytes resident in the geometry cache
            uint32_t geometryEntries = 0;         // distinct geometry cache entries alive
        };

        // One planned submit of a draw (recorded by Draw, also with a null program).
        struct CellFieldPlannedSubmit
        {
            bool backdrop = false;   // true: backdrop rectangle, false: the layer itself
            bool hillshade = false;  // layer submit only: hillshade active (elevation texture bound)
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
                              ICellFieldGpuGeometryFactory& geometryFactory, ICellFieldGpuTextureFactory& textureFactory,
                              ICellFieldGpuColourMapFactory& colourMapFactory);
            ~CellFieldRenderer();

            void SetCamera(const Dia::Graphics::Camera2D& camera);
            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size);
            // The view transform SetupView submits (shared Ortho2D helper). bgfx-free: callable without a GPU.
            void GetViewMatrix(float outMatrix[16], bool homogeneousDepth = false) const;
            unsigned short GetViewId() const { return mViewId; }

            // Processes queued Create/Update/Destroy commands. Called before drawing each frame.
            void DrainRegistry();
            // Drains the registry, then draws requests in order.
            void Draw(const Dia::Graphics::CellFieldFrameData& frame);
            // Frees every GPU object. Idempotent.
            void Shutdown();

            CellFieldRendererStats GetRendererStats() const;
            // Submits planned by the most recent Draw call, in submission order.
            const std::vector<CellFieldPlannedSubmit>& GetLastPlannedSubmits() const { return mLastPlan; }

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
                float                             cellSize = 1.0f;
                CellFieldBounds                   bounds;             // world rect covering every hex corner (backdrop quad)
                bool                              hasBounds = false;

                // Category layers keep a CPU copy of the last Update (<= 64 KB at 128x128) so
                // out-of-range ids can be counted without a GPU readback.
                std::vector<float>                values;
                uint32_t                          valueVersion = 0;   // bumped per accepted Update
                // Last out-of-range scan key; a scan reruns only when one of these changes.
                bool                              scanned = false;
                uint32_t                          scanValueVersion = 0;
                uint32_t                          scanPaletteId = 0;
                uint32_t                          scanPaletteRevision = 0;
            };

            struct ColourMapTexture
            {
                CellFieldGpuColourMapHandle handle = 0;
                uint32_t                    revision = 0;
            };

            // Uploads the baked palette only if the id is new or its revision changed.
            bool EnsurePaletteTexture(uint32_t id, const Dia::Graphics::CellFieldPaletteData& palette, CellFieldGpuColourMapHandle& outHandle);
            // Same for ramps (256 texels, linear filter).
            bool EnsureRampTexture(uint32_t id, const Dia::Graphics::CellFieldRampData& ramp, CellFieldGpuColourMapHandle& outHandle);
            void CountOutOfRange(Layer& layer, uint32_t paletteId, const Dia::Graphics::CellFieldPaletteData& palette);
            void SetupView();
            void SubmitLayer(const Layer& layer, const CellFieldDrawParams& params,
                             CellFieldGpuColourMapHandle colourMap, CellFieldGpuTextureHandle elevationTexture);
            void SubmitBackdrop(const Layer& layer, const CellFieldDrawParams& params);
            void ReleaseLayer(Layer& layer);
            // 1x1 R32F stand-in bound at stage 2 when hillshade is off (the shader samples it
            // unconditionally). Created lazily once, destroyed at Shutdown. False if creation fails.
            bool EnsureDummyElevation();

            unsigned short                    mViewId;
            ShaderProgram*                    mProgram;   // not owned
            Dia::Graphics::CellFieldRegistry* mRegistry;  // not owned
            Dia::Maths::Vector2D              mCanvasSize;
            Dia::Graphics::Camera2D           mCamera;

            BgfxCellFieldGpuGeometryFactory      mOwnedGeometryFactory;
            BgfxCellFieldGpuTextureFactory       mOwnedTextureFactory;
            BgfxCellFieldGpuColourMapFactory     mOwnedColourMapFactory;
            ICellFieldGpuTextureFactory&         mTextureFactory;
            ICellFieldGpuColourMapFactory&       mColourMapFactory;
            std::unordered_map<uint32_t, ColourMapTexture> mPaletteTextures;   // by colourMap id
            std::unordered_map<uint32_t, ColourMapTexture> mRampTextures;      // by colourMap id
            std::unordered_set<uint32_t>         mWarnedUnknownColourMaps;
            std::unordered_set<uint32_t>         mWarnedOutOfRangePalettes;   // one warning per palette id
            bool                                 mShutdownLogged = false;
            uint64_t                             mLayersCreated = 0;
            uint64_t                             mLayersReleased = 0;
            CellFieldRendererStats               mStats;
            std::vector<CellFieldPlannedSubmit>  mLastPlan;
            Dia::Graphics::HexFlatTopOddQBuilder mHexBuilder;
            CellFieldGeometryCache               mGeometryCache;
            std::unordered_map<uint16_t, Layer>  mLayers;  // by handle index
            CellFieldGpuTextureHandle            mDummyElevation = 0;
            bool                                 mHasDummyElevation = false;

            CellFieldRenderer(const CellFieldRenderer&) = delete;
            CellFieldRenderer& operator=(const CellFieldRenderer&) = delete;
        };

    } // namespace Bgfx
} // namespace Dia
