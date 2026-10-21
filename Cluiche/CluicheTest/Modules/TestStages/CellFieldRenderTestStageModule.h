#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaStreams/StreamReader.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/RenderFence.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaMaths/Vector/Vector2D.h>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/Camera2DModule.h"
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#endif

namespace Dia { namespace Graphics { class CellFieldRegistry; } }

namespace CluicheTest {

// Visual proof for the CellField renderer (cell-field-renderer spec Task 22).
// Runs a fixed sequence of static scenes; each scene is captured via DIA_CAPTURE under its own
// tag (out/CluicheTest/captures/run/<tag>.png). A probes file with the world-space positions and
// expected colours is written next to the captures; the E2E scenario does the pixel checks.
class CellFieldRenderTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "CellField renderer visual proof: draw order, hillshade golden, camera parity via frame capture";
    explicit CellFieldRenderTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~CellFieldRenderTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 1800; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    static constexpr unsigned int kMaxScenes = 16;

    struct Scene
    {
        const char* tag = "";
        float camX = 0.0f;
        float camY = 0.0f;
        float zoom = 1.0f;

        bool drawField = false;
        Dia::Core::CellFieldDrawRequest request;

        bool drawSprite = false;
        Dia::Maths::Vector2D spritePos;
        float spriteScale = 1.0f;

        bool drawLine = false;
        Dia::Maths::Vector2D lineA;
        Dia::Maths::Vector2D lineB;

        bool drawCircle = false;
        Dia::Maths::Vector2D circlePos;
        float circleRadius = 0.0f;

        uint64_t uploadsAtCapture = 0; // registry stats snapshot when the capture was requested
    };

    void BuildScenes();
    void ApplyScene(const Scene& scene);
    void WriteProbes();
    void Fail(const char* why);

    Dia::ApplicationFlow::ServiceStreamReader<Dia::Graphics::CellFieldRegistry> mRegistryStream{this, "KernelCellFieldRegistry"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler> mTextureStream{this, "KernelTextureHandler"};
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mSceneOut{this, "SimScene"};
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::RenderFence> mFence{this, "RenderToSim"};
    Dia::Graphics::FrameData mSceneFrame;

    Scene mScenes[kMaxScenes];
    unsigned int mSceneCount = 0;
    int mActiveScene = -1;
    bool mBuilt = false;
    bool mDone = false;

    // Phase state machine.
    enum class Phase { Settle, WaitCapture, Linger, Done };
    Phase mPhase = Phase::Settle;
    unsigned int mPhaseFrames = 0;
    uint64_t mPhaseFence = 0;
    uint64_t mLastFence = 0;
    unsigned int mWrittenFrames = 0;
    unsigned int mRetries = 0;
    bool mAllCaptured = false;

    // Layers (valid after BuildScenes).
    Dia::Core::CellFieldLayerHandle mLayerA;
    Dia::Core::CellFieldLayerHandle mLayerPattern;
    Dia::Core::CellFieldLayerHandle mLayerUniform;
    Dia::Core::CellFieldLayerHandle mLayerCam;
    Dia::Core::CellFieldLayerHandle mElevHill;
    Dia::Core::CellFieldLayerHandle mElevFlat;

    // Geometry published to the probes file.
    float mGridBCentreX = 0.0f;
    float mGridBCentreY = 0.0f;
    float mGridBSize = 24.0f;
    float mSavedCamX = 0.0f;
    float mSavedCamY = 0.0f;
    float mSavedZoom = 1.0f;
    bool mCameraSaved = false;

#ifdef DIA_DEBUG
    friend class CellFieldDebugLayer;
    class CellFieldDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit CellFieldDebugLayer(const CellFieldRenderTestStageModule* module) : mModule(module) {}
        Dia::Core::StringCRC GetLayerName() const override { return Dia::Core::StringCRC("cellfieldrender.scene"); }
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const CellFieldRenderTestStageModule* mModule;
    };
    CellFieldDebugLayer mDebugLayer{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Camera2DModule> mCameraRef{this};
    bool mLayerRegistered = false;
#endif
};

} // namespace CluicheTest
