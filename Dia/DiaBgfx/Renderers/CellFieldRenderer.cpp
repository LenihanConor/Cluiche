////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx/Renderers/CellFieldRenderer.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <bgfx/bgfx.h>

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
            , mTextureFactory(mOwnedTextureFactory)
            , mGeometryCache(mOwnedGeometryFactory)
        {
            mGeometryCache.AddBuilder(&mHexBuilder);
        }

        CellFieldRenderer::CellFieldRenderer(unsigned short viewId, ShaderProgram* program, Dia::Graphics::CellFieldRegistry* registry,
                                             ICellFieldGpuGeometryFactory& geometryFactory, ICellFieldGpuTextureFactory& textureFactory)
            : mViewId(viewId), mProgram(program), mRegistry(registry)
            , mTextureFactory(textureFactory)
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

            const auto& requests = frame.GetCellFieldFrameData();
            for (unsigned int i = 0; i < requests.Size(); ++i)
            {
                const Dia::Core::CellFieldDrawRequest& request = requests[i];
                if (!mRegistry->ResolveHandle(request.layer))
                    continue;   // stale: counted by the registry
                auto it = mLayers.find(request.layer.index);
                if (it == mLayers.end() || it->second.generation != request.layer.generation)
                    continue;
                if (!mProgram)
                    continue;   // no GPU path (tests)
                SubmitLayer(request, it->second);
            }
        }

        void CellFieldRenderer::SubmitLayer(const Dia::Core::CellFieldDrawRequest& request, const Layer& layer)
        {
            // Tasks 10-13: set uniforms, bind value/palette/ramp/elevation textures, set geometry
            // buffers and state, then submit. Must never upload value data.
            (void)request; (void)layer; (void)mViewId;
        }

        void CellFieldRenderer::Shutdown()
        {
            for (auto& kv : mLayers)
                mTextureFactory.Destroy(kv.second.valueTexture);
            mLayers.clear();
            mGeometryCache.Shutdown();
        }

    } // namespace Bgfx
} // namespace Dia
