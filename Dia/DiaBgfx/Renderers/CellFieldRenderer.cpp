////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/CellFieldRenderer.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include "DiaBgfx/Resources/ShaderProgram.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace Dia
{
    namespace Bgfx
    {
        // ---- Real bgfx texture factory ----------------------------------------

        bool BgfxCellFieldGpuTextureFactory::Create(uint16_t width, uint16_t height, CellFieldGpuTextureHandle& outHandle)
        {
            const uint64_t flags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
            const bgfx::TextureHandle t = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R32F, flags);
            if (!bgfx::isValid(t))
                return false;
            outHandle = t.idx;
            Dim d;
            d.w = width;
            d.h = height;
            mDims[outHandle] = d;
            return true;
        }

        void BgfxCellFieldGpuTextureFactory::Update(CellFieldGpuTextureHandle handle, const float* values, uint32_t count)
        {
            auto it = mDims.find(handle);
            if (it == mDims.end() || count != static_cast<uint32_t>(it->second.w) * it->second.h)
                return;
            // bgfx::copy duplicates the data, so the caller's (transient) buffer may be reused immediately.
            bgfx::updateTexture2D(bgfx::TextureHandle{ static_cast<uint16_t>(handle) }, 0, 0, 0, 0,
                                  it->second.w, it->second.h, bgfx::copy(values, count * static_cast<uint32_t>(sizeof(float))));
        }

        void BgfxCellFieldGpuTextureFactory::Destroy(CellFieldGpuTextureHandle handle)
        {
            mDims.erase(handle);
            bgfx::destroy(bgfx::TextureHandle{ static_cast<uint16_t>(handle) });
        }

        // ---- Real bgfx colour-map factory ---------------------------------------

        namespace
        {
            // Packed 0xRRGGBBAA -> memory bytes R,G,B,A (what an RGBA8 texture expects).
            void ToRgba8Bytes(const uint32_t* texels, uint32_t count, uint8_t* out)
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    out[i * 4 + 0] = static_cast<uint8_t>(texels[i] >> 24);
                    out[i * 4 + 1] = static_cast<uint8_t>(texels[i] >> 16);
                    out[i * 4 + 2] = static_cast<uint8_t>(texels[i] >> 8);
                    out[i * 4 + 3] = static_cast<uint8_t>(texels[i]);
                }
            }
        }

        bool BgfxCellFieldGpuColourMapFactory::CreateRgba8(uint32_t width, const uint32_t* texels, uint32_t count, bool linearFilter, CellFieldGpuColourMapHandle& outHandle)
        {
            if (count != width)
                return false;
            const bgfx::Memory* mem = bgfx::alloc(count * 4u);
            ToRgba8Bytes(texels, count, mem->data);
            uint64_t flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
            if (!linearFilter)
                flags |= BGFX_SAMPLER_POINT;
            const bgfx::TextureHandle t = bgfx::createTexture2D(static_cast<uint16_t>(width), 1, false, 1, bgfx::TextureFormat::RGBA8, flags, mem);
            if (!bgfx::isValid(t))
                return false;
            outHandle = t.idx;
            mWidths[outHandle] = width;
            return true;
        }

        void BgfxCellFieldGpuColourMapFactory::UpdateRgba8(CellFieldGpuColourMapHandle handle, const uint32_t* texels, uint32_t count)
        {
            auto it = mWidths.find(handle);
            if (it == mWidths.end() || it->second != count)
                return;
            const bgfx::Memory* mem = bgfx::alloc(count * 4u);
            ToRgba8Bytes(texels, count, mem->data);
            bgfx::updateTexture2D(bgfx::TextureHandle{ static_cast<uint16_t>(handle) }, 0, 0, 0, 0, static_cast<uint16_t>(count), 1, mem);
        }

        void BgfxCellFieldGpuColourMapFactory::Destroy(CellFieldGpuColourMapHandle handle)
        {
            mWidths.erase(handle);
            bgfx::destroy(bgfx::TextureHandle{ static_cast<uint16_t>(handle) });
        }

        // ---- Command sink ------------------------------------------------------

        class CellFieldRenderer::Sink : public Dia::Graphics::ICellFieldCommandSink
        {
        public:
            explicit Sink(CellFieldRenderer& r) : mR(r) {}

            void OnCreate(Dia::Core::CellFieldLayerHandle handle, const Dia::Graphics::CellGridDesc& desc, Dia::Graphics::CellFieldValueKind kind) override
            {
                auto existing = mR.mLayers.find(handle.index);
                if (existing != mR.mLayers.end())
                {
                    mR.ReleaseLayer(existing->second);   // defensive: slot reused without Destroy
                    mR.mLayers.erase(existing);
                }

                const CellFieldGeometryCache::Entry* entry = mR.mGeometryCache.Acquire(desc);
                if (!entry)
                {
                    DIA_LOG_ERROR("CFL", "CellFieldRenderer: geometry acquire failed for layer %u", static_cast<unsigned>(handle.index));
                    return;
                }
                Layer layer;
                layer.generation = handle.generation;
                layer.width = desc.width;
                layer.height = desc.height;
                layer.cellSize = desc.cellSize;
                layer.kind = kind;
                layer.geometryKey = CellFieldGeometryCache::MakeKey(desc);
                layer.geometry = entry->gpu;
                if (!mR.mTextureFactory.Create(desc.width, desc.height, layer.valueTexture))
                {
                    DIA_LOG_ERROR("CFL", "CellFieldRenderer: value texture create failed for layer %u", static_cast<unsigned>(handle.index));
                    mR.mGeometryCache.Release(layer.geometryKey);
                    return;
                }
                mR.mLayers[handle.index] = layer;
            }

            void OnUpdate(Dia::Core::CellFieldLayerHandle handle, const float* values, uint32_t count) override
            {
                auto it = mR.mLayers.find(handle.index);
                if (it == mR.mLayers.end() || it->second.generation != handle.generation)
                    return;   // stale: skipped
                if (count != static_cast<uint32_t>(it->second.width) * it->second.height)
                    return;
                mR.mTextureFactory.Update(it->second.valueTexture, values, count);
                if (it->second.kind == Dia::Graphics::CellFieldValueKind::Category)
                {
                    it->second.values.assign(values, values + count);
                    ++it->second.valueVersion;
                }
            }

            void OnDestroy(Dia::Core::CellFieldLayerHandle handle) override
            {
                auto it = mR.mLayers.find(handle.index);
                if (it == mR.mLayers.end() || it->second.generation != handle.generation)
                    return;
                mR.ReleaseLayer(it->second);
                mR.mLayers.erase(it);
            }

        private:
            CellFieldRenderer& mR;
        };

        // ---- Renderer ----------------------------------------------------------

        CellFieldRenderer::CellFieldRenderer(unsigned short viewId, ShaderProgram* program, Dia::Graphics::CellFieldRegistry* registry)
            : mViewId(viewId), mProgram(program), mRegistry(registry)
            , mTextureFactory(mOwnedTextureFactory), mColourMapFactory(mOwnedColourMapFactory)
            , mGeometryCache(mOwnedGeometryFactory)
        {
            mGeometryCache.AddBuilder(&mHexBuilder);
        }

        CellFieldRenderer::CellFieldRenderer(unsigned short viewId, ShaderProgram* program, Dia::Graphics::CellFieldRegistry* registry,
                                             ICellFieldGpuGeometryFactory& geometryFactory, ICellFieldGpuTextureFactory& textureFactory,
                                             ICellFieldGpuColourMapFactory& colourMapFactory)
            : mViewId(viewId), mProgram(program), mRegistry(registry)
            , mTextureFactory(textureFactory), mColourMapFactory(colourMapFactory)
            , mGeometryCache(geometryFactory)
        {
            mGeometryCache.AddBuilder(&mHexBuilder);
        }

        CellFieldRenderer::~CellFieldRenderer()
        {
            Shutdown();
        }

        void CellFieldRenderer::SetCamera(const Dia::Graphics::Camera2D& camera) { mCamera = camera; }
        void CellFieldRenderer::OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) { mCanvasSize = size; }

        void CellFieldRenderer::ReleaseLayer(Layer& layer)
        {
            mTextureFactory.Destroy(layer.valueTexture);
            mGeometryCache.Release(layer.geometryKey);
        }

        void CellFieldRenderer::DrainRegistry()
        {
            DIA_PROFILE_SCOPE("CellField.DrainRegistry", Dia::Observation::Profile::Category::kDiaGraphics);
            if (!mRegistry)
                return;
            Sink sink(*this);
            mRegistry->Drain(sink);
        }

        void CellFieldRenderer::Draw(const Dia::Graphics::CellFieldFrameData& frame)
        {
            DrainRegistry();
            DIA_PROFILE_SCOPE("CellField.Draw", Dia::Observation::Profile::Category::kDiaGraphics);
            if (!mRegistry)
                return;

            bool viewReady = false;
            const auto& requests = frame.GetCellFieldFrameData();
            for (unsigned int i = 0; i < requests.Size(); ++i)
            {
                const Dia::Core::CellFieldDrawRequest& request = requests[i];
                if (!mRegistry->ResolveHandle(request.layer))
                    continue;   // stale: counted by the registry
                auto it = mLayers.find(request.layer.index);
                if (it == mLayers.end() || it->second.generation != request.layer.generation)
                    continue;
                Layer& layer = it->second;

                // Extension point (Task 11): Float layers map through a ramp.
                if (layer.kind != Dia::Graphics::CellFieldValueKind::Category)
                    continue;

                Dia::Graphics::CellFieldPaletteData palette;
                if (!mRegistry->GetPalette(request.colourMap, palette))
                {
                    ++mStats.unknownColourMaps;
                    if (mWarnedUnknownColourMaps.insert(request.colourMap).second)
                        DIA_LOG_WARNING("CFL", "CellFieldRenderer: unknown palette id 0x%08X, draw skipped", static_cast<unsigned>(request.colourMap));
                    continue;
                }

                CellFieldLayerInfo info;
                info.width = layer.width;
                info.height = layer.height;
                info.cellSize = layer.cellSize;
                info.kind = layer.kind;
                CellFieldDrawParams params;
                if (!BuildCellFieldDrawParams(request, info, &palette, nullptr, params))
                    continue;

                CellFieldGpuColourMapHandle colourMap = 0;
                if (!EnsurePaletteTexture(request.colourMap, palette, colourMap))
                    continue;
                CountOutOfRange(layer, request.colourMap, palette);

                if (!mProgram)
                    continue;   // no GPU path (tests): everything above still ran
                if (!viewReady)
                {
                    SetupView();
                    viewReady = true;
                }
                SubmitLayer(request, layer, params, colourMap);
            }
        }

        // Palette textures are keyed by colour-map id and carry the registry revision they were
        // baked from. A draw never uploads unless the id is new or the palette was re-registered,
        // so switching colourMap between draws costs nothing after each palette's first use.
        bool CellFieldRenderer::EnsurePaletteTexture(uint32_t id, const Dia::Graphics::CellFieldPaletteData& palette, CellFieldGpuColourMapHandle& outHandle)
        {
            uint32_t texels[Dia::Graphics::CellFieldRegistry::kMaxPaletteColours];
            auto it = mPaletteTextures.find(id);
            if (it != mPaletteTextures.end())
            {
                if (it->second.revision != palette.revision)
                {
                    Dia::Graphics::BakePalette(palette.colours, palette.count, palette.fallback, texels);
                    mColourMapFactory.UpdateRgba8(it->second.handle, texels, Dia::Graphics::CellFieldRegistry::kMaxPaletteColours);
                    it->second.revision = palette.revision;
                    ++mStats.colourMapUploads;
                }
                outHandle = it->second.handle;
                return true;
            }

            Dia::Graphics::BakePalette(palette.colours, palette.count, palette.fallback, texels);
            ColourMapTexture entry;
            if (!mColourMapFactory.CreateRgba8(Dia::Graphics::CellFieldRegistry::kMaxPaletteColours, texels,
                                               Dia::Graphics::CellFieldRegistry::kMaxPaletteColours, false, entry.handle))
            {
                DIA_LOG_ERROR("CFL", "CellFieldRenderer: palette texture create failed for id 0x%08X", static_cast<unsigned>(id));
                return false;
            }
            entry.revision = palette.revision;
            mPaletteTextures[id] = entry;
            ++mStats.colourMapUploads;
            mStats.colourMapsResident = static_cast<uint32_t>(mPaletteTextures.size());
            outHandle = entry.handle;
            return true;
        }

        // "A category id outside the palette draws the fallback colour and increments a counter."
        // Mechanism: the layer keeps a CPU copy of its last Update; the cells outside the bound
        // palette are counted by a scan that reruns only when (valueVersion, palette id, palette
        // revision) changes. A static layer costs one scan, not one per frame; the counter is
        // cumulative over scans (i.e. over distinct (values, palette) combinations drawn).
        void CellFieldRenderer::CountOutOfRange(Layer& layer, uint32_t paletteId, const Dia::Graphics::CellFieldPaletteData& palette)
        {
            if (layer.values.empty())
                return;
            if (layer.scanned && layer.scanValueVersion == layer.valueVersion &&
                layer.scanPaletteId == paletteId && layer.scanPaletteRevision == palette.revision)
                return;

            uint64_t outOfRange = 0;
            uint32_t index = 0;
            for (float v : layer.values)
            {
                if (!CellFieldPaletteIndex(palette, v, index))
                    ++outOfRange;
            }
            layer.scanned = true;
            layer.scanValueVersion = layer.valueVersion;
            layer.scanPaletteId = paletteId;
            layer.scanPaletteRevision = palette.revision;
            mStats.outOfRangeCategoryCells += outOfRange;
        }

        CellFieldRendererStats CellFieldRenderer::GetRendererStats() const
        {
            return mStats;
        }

        void CellFieldRenderer::SetupView()
        {
            const float cw = mCanvasSize.X();
            const float ch = mCanvasSize.Y();
            if (cw < 1.0f || ch < 1.0f)
                return;
            const float zoom  = mCamera.GetZoom() > 0.0f ? mCamera.GetZoom() : 1.0f;
            const float halfW = (cw * 0.5f) / zoom;
            const float halfH = (ch * 0.5f) / zoom;
            const float cx    = mCamera.GetPosition().X();
            const float cy    = mCamera.GetPosition().Y();

            float ortho[16];
            bx::mtxOrtho(ortho, cx - halfW, cx + halfW, cy + halfH, cy - halfH,
                         0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(mViewId, nullptr, ortho);
            bgfx::setViewRect(mViewId, 0, 0, static_cast<uint16_t>(cw), static_cast<uint16_t>(ch));
        }

        void CellFieldRenderer::SubmitLayer(const Dia::Core::CellFieldDrawRequest& request, const Layer& layer,
                                            const CellFieldDrawParams& params, CellFieldGpuColourMapHandle colourMap)
        {
            (void)request;
            if (!mProgram || !mProgram->IsValid())
                return;

            struct Uniforms
            {
                bgfx::UniformHandle params0, params1, params2, gridInfo, fallback, backdrop, lightDir;
                bgfx::UniformHandle sColour, sValue, sElevation;
                bool created = false;
            };
            static Uniforms u;
            if (!u.created)
            {
                u.params0   = bgfx::createUniform("u_params0", bgfx::UniformType::Vec4);
                u.params1   = bgfx::createUniform("u_params1", bgfx::UniformType::Vec4);
                u.params2   = bgfx::createUniform("u_params2", bgfx::UniformType::Vec4);
                u.gridInfo  = bgfx::createUniform("u_gridInfo", bgfx::UniformType::Vec4);
                u.fallback  = bgfx::createUniform("u_fallbackColour", bgfx::UniformType::Vec4);
                u.backdrop  = bgfx::createUniform("u_backdropColour", bgfx::UniformType::Vec4);
                u.lightDir  = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
                u.sColour   = bgfx::createUniform("u_paletteOrRamp", bgfx::UniformType::Sampler);
                u.sValue    = bgfx::createUniform("u_valueTexture", bgfx::UniformType::Sampler);
                u.sElevation = bgfx::createUniform("u_elevationTexture", bgfx::UniformType::Sampler);
                u.created = true;
            }

            bgfx::setUniform(u.params0, params.params0);
            bgfx::setUniform(u.params1, params.params1);
            bgfx::setUniform(u.params2, params.params2);
            bgfx::setUniform(u.gridInfo, params.gridInfo);
            bgfx::setUniform(u.fallback, params.fallbackColour);
            bgfx::setUniform(u.backdrop, params.backdropColour);
            bgfx::setUniform(u.lightDir, params.lightDir);

            const bgfx::TextureHandle value = { static_cast<uint16_t>(layer.valueTexture) };
            bgfx::setTexture(0, u.sColour, bgfx::TextureHandle{ static_cast<uint16_t>(colourMap) });
            bgfx::setTexture(1, u.sValue, value);
            // Extension point (Task 13): the elevation texture. The shader samples stage 2
            // unconditionally (result unused when hasElevation == 0), so bind a valid texture.
            bgfx::setTexture(2, u.sElevation, value);

            bgfx::setVertexBuffer(0, bgfx::VertexBufferHandle{ BgfxCellFieldGpuGeometryFactory::VertexBuffer(layer.geometry) });
            bgfx::setIndexBuffer(bgfx::IndexBufferHandle{ BgfxCellFieldGpuGeometryFactory::IndexBuffer(layer.geometry) });
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(mViewId, bgfx::ProgramHandle{ mProgram->GetProgramHandle() });
        }

        void CellFieldRenderer::Shutdown()
        {
            for (auto& kv : mLayers)
                mTextureFactory.Destroy(kv.second.valueTexture);
            mLayers.clear();
            for (auto& kv : mPaletteTextures)
                mColourMapFactory.Destroy(kv.second.handle);
            mPaletteTextures.clear();
            mStats.colourMapsResident = 0;
            mGeometryCache.Shutdown();
        }

    } // namespace Bgfx
} // namespace Dia
