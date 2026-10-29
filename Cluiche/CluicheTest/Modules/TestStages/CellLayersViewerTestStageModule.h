#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCellLayers/CellLayersModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <stdint.h>
#include <vector>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/Camera2DModule.h"
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#endif

namespace Dia { namespace Graphics { class CellFieldRegistry; } }
namespace Dia { namespace ValueSchema { class ValueSchemaService; } }

namespace CluicheTest {

// Hosts the DiaCellLayers viewer (CellLayersModule registry + CellLayerViewerDomain + LayerUploader) over synthetic
// layers on two hex grids so the "Cell Layers" debug panel card can be seen and E2E-driven.
//  - Grid A (66x42): float "Temperature" (Climate, default base, hotkey t), float "Rainfall" (Climate, hotkey r),
//    category "Biome" (Terrain, hotkey b), overlay "Grid edges".
//  - Grid B (20x12): float "Elevation" (hotkey e). Two grids => the panel shows the grid selector.
//  - Values are deterministic functions of the cell coordinates. Temperature version() is a counter; it only moves when
//    the animation flag is on (default OFF; DiaAPI command "cellviewer.animate 1|0", step every kAnimateEveryFrames).
//  - Schema entries live in Stages/CellLayersViewerTestStage/celllayers_viewer.schema.json (loaded by ValueSchemaModule).
// Release builds (no debug domain) report passed immediately.
class CellLayersViewerTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "DiaCellLayers viewer host: synthetic float/category layers on two hex grids, overlay, hover, debug panel card";
    explicit CellLayersViewerTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~CellLayersViewerTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 3600; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    static constexpr int          kBigW = 66;
    static constexpr int          kBigH = 42;
    static constexpr int          kSmallW = 20;
    static constexpr int          kSmallH = 12;
    static constexpr float        kCellSize = 24.0f;
    static constexpr unsigned int kHoldFrames = 1200;       // frames after ready before ReportPassed (automation mode leaves the stage then)
    static constexpr unsigned int kAnimateEveryFrames = 15; // temperature version step when animating

    struct GridStorage
    {
        std::vector<float>   centres;
        std::vector<int32_t> neighbours;
        std::vector<float>   baseValue;  // per-cell static "latitude" style value in [0,1]
        Dia::Graphics::CellGridDesc desc;
        Dia::Maths::Vector2D boundsMin;
        Dia::Maths::Vector2D boundsMax;
        int width = 0;
        int height = 0;
    };

    static void BuildGrid(GridStorage& g, int w, int h, float size);

    Dia::ApplicationFlow::ServiceStreamReader<Dia::Graphics::CellFieldRegistry> mCellFieldStream{this, "KernelCellFieldRegistry"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::ValueSchema::ValueSchemaService> mSchemaStream{this, "ValueSchemaService"};

    Dia::ApplicationFlow::ServiceStreamReader<Dia::CellLayers::CellLayerRegistry> mLayerRegistryStream{this, Dia::CellLayers::CellLayersModule::kServiceStreamId};

    GridStorage mBig;
    GridStorage mSmall;
    uint64_t    mTempVersion = 1;
    uint64_t    mRainVersion = 1;
    uint64_t    mBiomeVersion = 1;
    uint64_t    mElevVersion = 1;
    bool        mAnimate = false;
    unsigned int mAnimateCounter = 0;
    unsigned int mReadyFrames = 0;
    bool        mReady = false;
    bool        mCommandRegistered = false;

    void ReadTemperature(float* out, uint32_t count);
    void ReadRainfall(float* out, uint32_t count);
    void ReadBiome(float* out, uint32_t count);
    void ReadElevation(float* out, uint32_t count);
    uint64_t VersionTemperature() { return mTempVersion; }
    uint64_t VersionRainfall()    { return mRainVersion; }
    uint64_t VersionBiome()       { return mBiomeVersion; }
    uint64_t VersionElevation()   { return mElevVersion; }

    float TemperatureAt(uint32_t index) const;
    float RainfallAt(uint32_t index) const;

#ifdef DIA_DEBUG
    bool BuildViewer();
    void TeardownViewer();
    void DrawGridEdges(Dia::Core::IDebugDraw& draw, const void* selection);
    Dia::CellLayers::CellPickerView GetPickerView();
    void FitCameraToActiveGrid();

    friend class ViewerDebugLayer;
    class ViewerDebugLayer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit ViewerDebugLayer(CellLayersViewerTestStageModule* module) : mModule(module) {}
        Dia::Core::StringCRC GetLayerName() const override { return Dia::Core::StringCRC("celllayersviewer.update"); }
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        CellLayersViewerTestStageModule* mModule;
    };
    ViewerDebugLayer mDebugLayer{this};

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Camera2DModule> mCameraRef{this};

    Dia::CellLayers::CellLayerRegistry* mRegistry = nullptr; // owned by CellLayersModule
    Dia::Core::UniquePtr<Dia::CellLayers::LayerUploader> mUploader;
    Dia::Core::UniquePtr<Dia::CellLayers::CellLayerViewerDomain> mDomain;
    bool  mDomainRegistered = false;
    bool  mLayerRegistered = false;
    bool  mLayersRegistered = false;
    const Dia::Graphics::CellGridDesc* mFittedGrid = nullptr;
    bool  mCameraSaved = false;
    float mSavedCamX = 0.0f;
    float mSavedCamY = 0.0f;
    float mSavedZoom = 1.0f;
#endif
};

} // namespace CluicheTest
