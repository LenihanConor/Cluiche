////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaBgfx/Canvas.h>
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Dia { namespace Graphics3D { class FrameData3D; } }
namespace Dia { namespace Mesh3D { class Mesh3DAssetHandler; } }
namespace Dia { namespace Bgfx { class ShaderProgram; } }
namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace Dia
{
    namespace Bgfx3D
    {
        class MaterialRegistry;
        class MeshGpuCache;
        class MeshRenderer;
        class ShadowRenderer;
        class DebugGeometry3DRenderer;

        class Canvas3D : public Dia::Bgfx::Canvas
        {
        public:
            Canvas3D();
            ~Canvas3D() override;

            void StartFrame(const Dia::Graphics::FrameData& frame) override;
            void Shutdown() override;
            void ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData);

            MaterialRegistry* GetMaterialRegistry();
            MeshGpuCache*     GetMeshGpuCache();

            void SetMeshHandler(Dia::Mesh3D::Mesh3DAssetHandler* handler);

            // Pass metric gauges down to renderers. Call before the first StartFrame.
            void SetMetrics(Dia::Observation::Metric::Gauge* meshDrawCalls,
                            Dia::Observation::Metric::Gauge* gpuMeshCount);

            Dia::Observation::Health::HealthReporterBase& GetHealthReporter();

        private:
            void Init3DPrograms();

            // View IDs: 1=shadow  2=mesh  3=debug3d
            unsigned short    mMeshViewId;
            unsigned short    mShadowViewId;
            unsigned short    mDebug3DViewId;
            MaterialRegistry* mMaterialRegistry;  // owned
            MeshGpuCache*     mMeshGpuCache;      // owned
            MeshRenderer*     mMeshRenderer;      // owned
            ShadowRenderer*   mShadowRenderer;    // owned
            DebugGeometry3DRenderer* mDebugGeometry3DRenderer;  // owned
            Dia::Mesh3D::Mesh3DAssetHandler* mMeshHandler; // not owned

            Dia::Bgfx::ShaderProgram* mMeshProgram;     // owned
            Dia::Bgfx::ShaderProgram* mShadowProgram;  // owned
            Dia::Bgfx::ShaderProgram* mDebug3DProgram; // owned
            bool                      m3DInitialised;

            Dia::Observation::Metric::Gauge* mMetricMeshDrawCalls = nullptr;
            Dia::Observation::Metric::Gauge* mMetricGpuMeshCount  = nullptr;

            class Canvas3DHealthReporter : public Dia::Observation::Health::HealthReporterBase
            {
            public:
                Dia::Core::StringCRC GetReporterName() const override
                {
                    return Dia::Core::StringCRC("DiaBgfx3D.Canvas3D");
                }
                Dia::Observation::Health::Health Report() const override
                {
                    return HealthReporterBase::Report();
                }
            };
            Canvas3DHealthReporter mHealthReporter;

            Canvas3D(const Canvas3D&) = delete;
            Canvas3D& operator=(const Canvas3D&) = delete;
        };

    } // namespace Bgfx3D
} // namespace Dia
