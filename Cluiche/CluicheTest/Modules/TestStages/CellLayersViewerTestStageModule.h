#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCellLayers/CellLayersModule.h>
#include <DiaCellHistory/CellHistoryModule.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <atomic>
#include <chrono>
#include <stdint.h>
#include <vector>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/Camera2DModule.h"
#include "Modules/InputStreamModule.h"
#include "Modules/SimQueryHostModule.h"

#include <cstdio>
#include "Modules/TestStages/CellLayersStateRelayModule.h"
#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/ITopicSink.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/DebugServerTopicSink.h>
#include <DiaCellLayers/CellPicker.h>
#include <DiaCellHistory/CellHistoryDomain.h>
#include <DiaCellHistory/SnapshotCellCache.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/SnapshotCommands.h>
#include <DiaCellHistory/SnapshotObservation.h>
#include <DiaCellHistory/HistoryExportCommands.h>
#include <DiaCellHistory/ExportObservation.h>
#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/QueryObservation.h>
#include <DiaCellHistory/HistoryTopics.h>
#include <DiaCellHistory/TopicObservation.h>
#include "Modules/TestStages/CellLayersSnapshotStore.h"
#include <DiaCellHistory/HistoryValueSource.h>
#include <DiaCellHistory/ViewHistorySourceAdapter.h>
#include "Modules/TestStages/CellLayersPointerQueue.h"
#include "Modules/TestStages/CellLayersHoverOverride.h"
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
    static constexpr unsigned int kHoldFrames = 60000;      // frames after ready before ReportPassed; long enough that the E2E smoke scenario (~60 s) finishes first
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

    // Consumes the recorder CellHistoryModule publishes (SimPU). The stage drives Sample on a timer and the history test command.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::CellHistory::CellHistoryRecorder> mHistoryStream{this, Dia::CellHistory::CellHistoryModule::kServiceStreamId};

    GridStorage mBig;
    GridStorage mSmall;
    uint64_t    mTempVersion = 1;
    uint64_t    mRainVersion = 1;
    uint64_t    mBiomeVersion = 1;
    uint64_t    mElevVersion = 1;
    std::atomic<bool> mAnimate{ false };  // written on command threads, read on SimPU
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
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::InputStreamModule> mInputRef{this};
    bool  mPointerFrameLogged = false;
    bool  mInjectLogged = false;
    bool  mInjectCommandsRegistered = false;
    PointerFrameQueue mPointerQueue;  // test-only injected frames (E2E), one popped per Sim frame
    HoverOverride mHoverOverride;     // test-only hover cursor (E2E); the viewer sees it through HoverOverrideDraw
    bool RegisterInjectionCommands();
    void UnregisterInjectionCommands();

    void SubmitPointerInput();

    // Selection Inspector detail provider for grid A ("Stage"): deterministic rows derived from the cell index.
    void BuildCellDetails(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& out);
    bool  mDetailsRegistered = false;

    Dia::CellLayers::CellLayerRegistry* mRegistry = nullptr; // owned by CellLayersModule
    Dia::Core::UniquePtr<Dia::CellLayers::LayerUploader> mUploader;
    Dia::Core::UniquePtr<Dia::CellLayers::CellLayerViewerDomain> mDomain;
    bool  mDomainRegistered = false;

    // Panel state (Streams Migration Task 4): the DebugDomain registry is read on Main, so it gets the relay's snapshot adapter,
    // never mDomain. The relay (SimPU) publishes the viewer's GetJSONState to a FrameStream while Main asks for it.
    Dia::ApplicationFlow::ModuleRef<CellLayersStateRelayModule> mStateRelayRef{this};
    Dia::VisualDebugger::IDebugDomain* mPanelDomain = nullptr;   // owned by the relay; null when not attached

    // Editor mirror (Feature 5): topic cells.view.state + four mirror queries, driven from the viewer's Update on SimPU.
    // Teardown order: queries unregistered, then queries/publisher/sink/clock destroyed, before the viewer.
    bool  BuildViewerCommands();
    bool  BuildMirror(const Dia::ValueSchema::ValueSchemaService* schema);
    void  TeardownMirror();
    class SteadyMonotonicClock : public Dia::CellLayers::IMonotonicClock
    {
    public:
        uint64_t NowMs() const override
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }
    };
    SteadyMonotonicClock mMirrorClock;
    Dia::Core::UniquePtr<Dia::CellLayers::DebugServerTopicSink> mMirrorSink;
    Dia::Core::UniquePtr<Dia::CellLayers::ViewStatePublisher> mMirrorPublisher;
    Dia::Core::UniquePtr<Dia::CellLayers::MirrorQueries> mMirrorQueries;
    // Forwards the mirror queries to the sim-hosted query service (runs handlers on this PU, SimPU). Dia::CellLayers cannot see Cluiche.
    class MirrorRegistrar : public Dia::CellLayers::IMirrorQueryRegistrar
    {
    public:
        explicit MirrorRegistrar(Cluiche::AppFlow::SimQueryHostModule& host) : mHost(host) {}
        bool Register(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::CellLayers::MirrorQueryHandler handler) override
        {
            return mHost.RegisterQuery(name, help, std::move(handler));
        }
        bool RegisterCommand(const char* name, const Dia::DebugServer::SimHelpInfo& help, Dia::CellLayers::MirrorQueryHandler handler) override
        {
            return mHost.RegisterCommand(name, help, std::move(handler));
        }
        void Unregister(const char* name) override { mHost.UnregisterQuery(name); }
    private:
        Cluiche::AppFlow::SimQueryHostModule& mHost;
    };
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mSimQueryHostRef{this};
    Dia::Core::UniquePtr<MirrorRegistrar> mMirrorRegistrar;  // non-null while the viewer commands / queries are registered

    // History (cells.history.* commands live in CellHistoryModule): its registrar must outlive CellHistoryModule::DoStop, which can
    // run after this stage's OnStop, so it is a separate adapter that is only destroyed with the stage.
    static constexpr unsigned int kHistorySampleEveryFrames = 30;
    Dia::ApplicationFlow::ModuleRef<Dia::CellHistory::CellHistoryModule> mHistoryModuleRef{this};
    Dia::Core::UniquePtr<MirrorRegistrar> mHistoryRegistrar;
    bool         mHistoryCommandRegistered = false;
    unsigned int mHistoryFrame = 0;
    void SampleHistory();
    bool RegisterHistoryCommand();
    void UnregisterHistoryCommand();
    // History scrub (DiaCellHistory Feature 2): built lazily on SimPU once the recorder is published (CellHistoryModule creates it on
    // its first update, after this stage's OnStart). Torn down in TeardownViewer before the layers/registry go.
    bool BuildHistoryScrub();
    void TeardownHistoryScrub();
    void BuildSnapshots(Dia::CellHistory::CellHistoryRecorder& recorder, const Dia::ValueSchema::ValueSchemaService* schema);
    void TeardownSnapshots();
    void UpdateHistoryScrub();
    bool HoveredCell(int32_t& q, int32_t& r) const
    {
        if (!mDomain) return false;
        const Dia::CellLayers::CellLayerViewerDomain::HoverInfo h = mDomain->Hover();
        q = h.q;
        r = h.r;
        return h.valid;
    }
    Dia::Core::StringCRC ActiveBaseLayer() const { return mDomain ? mDomain->ActiveBase() : Dia::Core::StringCRC::kZero; }
    Dia::CellHistory::CellHistoryRecorder* mScrubRecorder = nullptr;   // recorder the scrub objects were built over; null = not built
    Dia::Core::UniquePtr<Dia::CellHistory::HistoryViewState>       mScrubView;
    Dia::Core::UniquePtr<Dia::CellHistory::HistoryViewCommands>    mScrubViewCommands;
    Dia::Core::UniquePtr<Dia::CellHistory::CellHistoryCommands>    mScrubPanelCommands;   // executor for the panel "track" command only (not registered)
    Dia::Core::UniquePtr<Dia::CellHistory::HistoryValueSource>     mScrubSource;
    Dia::Core::UniquePtr<Dia::CellHistory::ViewHistorySourceAdapter> mScrubAdapter;
    Dia::Core::UniquePtr<Dia::CellHistory::CellHistoryDomain>      mScrubDomain;
    Dia::VisualDebugger::IDebugDomain* mScrubPanelDomain = nullptr;   // owned by the relay
    // Snapshots (DiaCellHistory Feature 3): stage-local store saving the animated state (temperature version, sample phase) to
    // out/CluicheTest/snapshots/<name>.state. Its queued request runs at the start of OnUpdate (the Sim frame boundary).
    Dia::Core::UniquePtr<CellLayersSnapshotStore>                  mSnapshotStore;
    Dia::Core::UniquePtr<Dia::CellHistory::SnapshotCellCache>      mSnapshotCache;   // 2-entry LRU of cell files for snapshot delta sides
    Dia::Core::UniquePtr<Dia::CellHistory::SnapshotManager>        mSnapshotManager;
    Dia::Core::UniquePtr<Dia::CellHistory::SnapshotObservation>    mSnapshotObservation;
    Dia::Core::UniquePtr<Dia::CellHistory::SnapshotCommands>       mSnapshotCommands;
    bool mSnapshotCommandsRegistered = false;
    // Export (DiaCellHistory Feature 5): cells.history.export, relative paths under out/CluicheTest/history. Torn down before the
    // snapshot manager it borrows. mWorldName backs the recorder's world-id provider (the stage name).
    Dia::Core::UniquePtr<Dia::CellHistory::ExportObservation>      mExportObservation;
    Dia::Core::UniquePtr<Dia::CellHistory::HistoryExportCommands>  mExportCommands;
    bool mExportCommandsRegistered = false;
    char mWorldName[64] = {};
    // Live topics (DiaCellHistory Feature 5): cells.history.aggregates / .events. Ticked on SimPU after sampling and published through
    // DebugServer::PublishTopicOrdered (any-thread, FIFO, drop-newest) - the server pointer comes from the "DebugServerService" stream.
    // Torn down with the snapshots (before the recorder / server go).
    class HistoryTopicPublisher : public Dia::CellHistory::ITopicPublisher
    {
    public:
        explicit HistoryTopicPublisher(Dia::DebugServer::DebugServer& server) : mServer(server) {}
        bool Publish(Dia::Core::StringCRC topic, const Json::Value& payload) override;
        uint32_t SubscriberCount() const override;
    private:
        Dia::DebugServer::DebugServer& mServer;
    };
    void BuildHistoryTopics(Dia::CellHistory::CellHistoryRecorder& recorder);
    Dia::Core::UniquePtr<HistoryTopicPublisher>                    mTopicPublisher;
    Dia::Core::UniquePtr<Dia::CellHistory::TopicObservation>       mTopicObservation;
    Dia::Core::UniquePtr<Dia::CellHistory::HistoryTopics>          mHistoryTopics;
    bool mScrubDomainRegistered = false;
    bool mScrubCommandsRegistered = false;
    bool mScrubBuildFailed = false;
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
