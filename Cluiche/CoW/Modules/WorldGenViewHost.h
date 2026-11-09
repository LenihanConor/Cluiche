// S-1 World Generation map view: hosts the DiaCellLayers viewer over the generated world (SimPU only, debug builds).
// Mirrors CluicheTest's CellLayersViewerTestStageModule (BuildViewer / SubmitPointerInput / FitCameraToActiveGrid /
// TeardownViewer) but owns the pieces itself: hex grid, layer registrations, overlays, detail provider, uploader, viewer
// domain, camera controller. The viewer's state-relay adapter is NOT registered as a panel card; the World Generation card
// drives the viewer through IWorldGenView.
//
// Per Sim frame the owner calls Update(ctx) (size check, input, camera, pointer frame). The viewer itself is driven by
// Draw(IDebugDraw&) from the debug layer manager (DebugLayer() is the IVisualDebugger to register): base resolve, version-gated
// upload (WorldLayerSource::Version == session.Revision()), CellField draw request, overlays, selection outline, hover.
//
// The registry borrows the grid desc by address. The grid lives in an optional and is re-emplaced in place on a size change,
// so the desc address is stable, the registry's selection entry is reused (it has a fixed number of slots) and layers are
// unregistered, flushed through one empty-state viewer update (the uploader releases its CellField layers) and re-registered.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-view.plan.md (task 7)
#pragma once

#ifdef DIA_DEBUG

#include "Modules/IWorldGenView.h"

#include <Shared/WorldGeneration/WorldCameraController.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/WorldHexGrid.h>
#include <Shared/WorldGeneration/WorldLayerSource.h>
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>

#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerUploader.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/SelectionInput.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <functional>
#include <optional>
#include <vector>

namespace Cluiche { namespace AppFlow { class Camera2DModule; class InputStreamModule; } }
namespace Dia { namespace ValueSchema { class ValueSchemaService; } }

namespace CoW {

class WorldGenViewHost : public IWorldGenView
{
public:
    // One frame of raw input, already mapped: the viewer's pointer frame plus the camera keys and wheel.
    struct InputSnapshot
    {
        Dia::CellLayers::SelectionInput::PointerFrame pointer;
        WorldGeneration::CameraInput                  camera;   // keys + wheel + mouse; drag and dt are filled by the host
    };

    struct Deps
    {
        // Required.
        Dia::CellLayers::CellLayerRegistry*         registry = nullptr;   // CellLayersModule's registry (SimPU)
        Dia::Graphics::CellFieldRegistry*           cellField = nullptr;  // "KernelCellFieldRegistry" stream
        // Optional.
        const Dia::ValueSchema::ValueSchemaService* schema = nullptr;     // "ValueSchemaService" stream; null = no schema data
        Dia::CellLayers::IMirrorQueryRegistrar*     registrar = nullptr;  // sim-hosted cells.* commands; null = not registered

        // Application modules (stage module fills these). When a port below is empty and the module is set, the host binds it.
        Cluiche::AppFlow::Camera2DModule*           camera = nullptr;
        Cluiche::AppFlow::InputStreamModule*        input = nullptr;

        // Ports (the unit-test seam, also what the module pointers are bound to).
        std::function<Dia::Maths::Vector2D()>        windowSize;                       // viewport pixels
        std::function<void(float&, float&, float&)>  getCamera;                        // centreX, centreY, zoom
        std::function<void(float, float, float)>     setCamera;                        // centreX, centreY, zoom
        std::function<bool(InputSnapshot&)>          readInput;                        // false = no input this frame
    };

    WorldGenViewHost(WorldGeneration::WorldGenSession& session, const Deps& deps);
    ~WorldGenViewHost() override;
    WorldGenViewHost(const WorldGenViewHost&)            = delete;
    WorldGenViewHost& operator=(const WorldGenViewHost&) = delete;

    // Registers layers, overlays, detail provider, builds uploader + viewer, registers the cells.* commands.
    bool Build();
    // Once per Sim frame: size check, input -> camera -> pointer frame.
    void Update(const Dia::SimTime::SimTimeContext& ctx);
    // Once per draw: drives the viewer (see header comment). Call from DebugLayer().
    void Draw(Dia::Core::IDebugDraw& draw);
    // Unregisters commands, overlays, layers and details, then destroys the viewer and uploader. Idempotent.
    void Teardown();
    bool IsBuilt() const { return mDomain.Get() != nullptr; }

    // Re-syncs with session.Dimensions(); a no-op when they match the grid. Update and Draw call this themselves.
    void OnSizeChanged();

    // The IVisualDebugger to register with the debug layer manager (its Draw calls WorldGenViewHost::Draw).
    Dia::Debug::IVisualDebugger& DebugLayer() { return mDebugLayer; }

    // Frames in which the viewer uploaded layer data (one per revision on idle sessions; a base switch also counts).
    uint32_t UploadCount() const { return mUploadFrames; }
    // Session revision whose data the viewer last uploaded; equals session.Revision() once the new world is visible.
    uint32_t LayerVersion() const { return mUploadedRevision; }
    uint64_t ReadFailures() const { return mUploader ? mUploader->ReadFailures() : 0u; }
    // Largest IDebugDraw::DroppedCount() seen at the end of a viewer draw (the DebugDraw budget overflow signal).
    uint32_t DroppedCount() const { return mMaxDropped; }

    const WorldGeneration::WorldHexGrid& Grid() const { return *mGrid; }
    Dia::CellLayers::CellLayerViewerDomain* Viewer() { return mDomain.Get(); }
    const WorldGeneration::WorldCameraController& Camera() const { return mCamera; }
    uint32_t RiverSegmentRebuilds() const { return mRiverRebuilds; }

    // IWorldGenView
    bool SelectCell(int x, int y) override;
    void ClearSelection() override;
    bool HasSelection() override;
    bool GetSelection(int& x, int& y) override;
    bool SetColourMode(const char* id) override;
    const char* GetColourMode() override;
    void ListColourModes(const std::function<void(const char* id)>& fn) override;
    void ListColourGroups(const std::function<void(const char* id, const char* group)>& fn) override;
    bool SetOverlayEnabled(const char* id, bool enabled) override;
    void ListOverlays(const std::function<void(const char* id, bool enabled, bool available)>& fn) override;
    void FillLegend(Json::Value& out) override;

    static const char* const kOverlayRivers;
    static const char* const kOverlayNeighbours;
    static const char* const kDefaultModeAlias;   // "terrain"

private:
    struct LayerBinding
    {
        WorldGenViewHost*                         host = nullptr;
        const WorldGeneration::WorldLayerDesc*    desc = nullptr;
        void     Read(float* out, uint32_t count);
        uint64_t Version();
    };

    class DebugLayerImpl : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit DebugLayerImpl(WorldGenViewHost& host) : mHost(host) {}
        Dia::Core::StringCRC GetLayerName() const override { return Dia::Core::StringCRC("worldgen.view"); }
        void Draw(Dia::Core::IDebugDraw& draw) override { mHost.Draw(draw); }
    private:
        WorldGenViewHost& mHost;
    };

    bool RegisterLayers();
    void UnregisterLayers();
    bool RegisterOverlays();
    void UnregisterOverlays();
    Dia::CellLayers::CellPickerView PickerView() const;
    void RefreshPorts();
    void FitCamera();
    bool SelectedCell(const void* selectionView, int& cell) const;
    const char* DefaultBaseId() const;
    void DrawRivers(Dia::Core::IDebugDraw& draw, const void* selection);
    void DrawNeighbours(Dia::Core::IDebugDraw& draw, const void* selection);
    void BuildCellDetails(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& out);

    WorldGeneration::WorldGenSession& mSession;
    Deps                              mDeps;

    std::optional<WorldGeneration::WorldHexGrid> mGrid;   // re-emplaced in place: the registry keeps its address
    std::vector<LayerBinding>         mBindings;          // one per descriptor, stable addresses
    Dia::Core::UniquePtr<Dia::CellLayers::LayerUploader>        mUploader;
    Dia::Core::UniquePtr<Dia::CellLayers::CellLayerViewerDomain> mDomain;
    bool mLayersRegistered   = false;
    bool mOverlaysRegistered = false;
    bool mDetailsRegistered  = false;
    bool mCommandsRegistered = false;
    DebugLayerImpl mDebugLayer{ *this };

    // Camera + input.
    WorldGeneration::WorldCameraController mCamera;
    bool  mNeedFit = true;
    float mFitW = 0.0f, mFitH = 0.0f;
    bool  mCameraSaved = false;
    float mSavedX = 0.0f, mSavedY = 0.0f, mSavedZoom = 1.0f;
    bool  mHavePrevMouse = false;
    float mPrevMouseX = 0.0f, mPrevMouseY = 0.0f;
    bool  mFirstPointerLogged = false;

    // Upload tracking.
    uint64_t mSeenUploads = 0;
    uint32_t mUploadFrames = 0;
    uint32_t mUploadedRevision = 0;
    uint32_t mMaxDropped = 0;

    // Caches (rebuilt only when their key changes).
    std::vector<WorldGeneration::LineSegment> mRiverSegments;
    uint32_t mRiverRevision = 0xFFFFFFFFu;
    uint32_t mRiverGridGeneration = 0;
    uint32_t mRiverRebuilds = 0;
    WorldGeneration::NeighbourOutline mOutline;
    int      mOutlineCell = -1;
    uint32_t mOutlineGridGeneration = 0;
    uint32_t mGridGeneration = 1;               // bumps whenever the grid is re-emplaced

    Json::Value mDetailCache;
    int      mDetailCell = -1;
    uint32_t mDetailRevision = 0xFFFFFFFFu;

    Json::Value mLegendCache;
    uint32_t mLegendBase = 0;
    uint32_t mLegendRevision = 0xFFFFFFFFu;
    uint64_t mLegendUploads = ~0ull;
    bool     mLegendValid = false;
};

} // namespace CoW

#endif // DIA_DEBUG
