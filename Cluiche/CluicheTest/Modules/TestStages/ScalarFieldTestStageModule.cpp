#include "Modules/TestStages/ScalarFieldTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaScalarField/CellIndex.h>

#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#endif

#include <cmath>

namespace CluicheTest {

const Dia::Core::StringCRC ScalarFieldTestStageModule::kTypeId("ScalarFieldTestStageModule");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ScalarFieldTestStageModule::ScalarFieldTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
    // Initialise all modifier values to 1.0 before mRedRulesField captures pointers.
    , mModifierGrid{}     // zero-init the array (will be overwritten in SetupGrid)
    , mBlueField(
        Dia::ScalarField::SquareFieldTopology(kGridW, kGridH, Dia::ScalarField::SquareConnectivity::k8Connected),
        Dia::ScalarField::UniformDecayPolicy(Dia::ScalarField::UniformDecayParams{0.8f, 0.05f}))
    , mRedField(
        Dia::ScalarField::SquareFieldTopology(kGridW, kGridH, Dia::ScalarField::SquareConnectivity::k8Connected),
        Dia::ScalarField::UniformDecayPolicy(Dia::ScalarField::UniformDecayParams{0.8f, 0.05f}))
    , mRedRulesField(
        Dia::ScalarField::SquareFieldTopology(kGridW, kGridH, Dia::ScalarField::SquareConnectivity::k8Connected),
        Dia::ScalarField::Adaptors::RulesPropagationPolicy<ScalarModifierFn>(
            ScalarModifierFn{mBlockedGrid, mModifierGrid}, 0.8f, 0.05f))
    , mCombinedField(
        Dia::ScalarField::SquareFieldTopology(kGridW, kGridH, Dia::ScalarField::SquareConnectivity::k8Connected),
        Dia::ScalarField::UniformDecayPolicy(Dia::ScalarField::UniformDecayParams{0.0f, 0.0f}))
{
    // Fill modifier array with 1.0 (default passable, non-swamp).
    for (int i = 0; i < kCellCount; ++i)
        mModifierGrid[i] = 1.0f;
}

ScalarFieldTestStageModule::~ScalarFieldTestStageModule() = default;

// ---------------------------------------------------------------------------
// TestStageModuleBase overrides
// ---------------------------------------------------------------------------

Dia::Core::StringCRC ScalarFieldTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("ScalarFieldTestStage");
}

const Dia::Core::StringCRC* ScalarFieldTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("scalarfield.fields_initialized"),
        Dia::Core::StringCRC("scalarfield.blue_steady_state"),
        Dia::Core::StringCRC("scalarfield.red_steady_state"),
        Dia::Core::StringCRC("scalarfield.walls_respected"),
        Dia::Core::StringCRC("scalarfield.box_write_burst"),
        Dia::Core::StringCRC("scalarfield.local_maxima_found"),
        Dia::Core::StringCRC("scalarfield.contested_zone_stable"),
        Dia::Core::StringCRC("scalarfield.gradient_non_zero_at_probe"),
    };
    outCount = 8;
    return names;
}

// ---------------------------------------------------------------------------
// SetupGrid — walls, swamp, clamp ranges
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::SetupGrid()
{
    // Clamp ranges
    mBlueField.SetClampRange(0.0f, 1.0f);
    mRedField.SetClampRange(0.0f, 1.0f);
    mRedRulesField.SetClampRange(0.0f, 1.0f);
    mCombinedField.SetClampRange(-1.0f, 1.0f);

    // Left wall: x=5, y in [3..11]
    for (int y = 3; y <= 11; ++y)
    {
        const Dia::ScalarField::CellIndex cell{5, y};
        mBlueField.SetBlocked(cell, true);
        mRedField.SetBlocked(cell, true);
        mRedRulesField.SetBlocked(cell, true);

        const int idx = y * kGridW + 5;
        mBlockedGrid[idx] = true;
        mModifierGrid[idx] = 0.0f;
    }

    // Right wall: x=14, y in [3..11]
    for (int y = 3; y <= 11; ++y)
    {
        const Dia::ScalarField::CellIndex cell{14, y};
        mBlueField.SetBlocked(cell, true);
        mRedField.SetBlocked(cell, true);
        mRedRulesField.SetBlocked(cell, true);

        const int idx = y * kGridW + 14;
        mBlockedGrid[idx] = true;
        mModifierGrid[idx] = 0.0f;
    }

    // Swamp strip: y==7, 6<=x<=13
    for (int x = 6; x <= 13; ++x)
    {
        const Dia::ScalarField::CellIndex cell{x, 7};
        mBlueField.SetStaticModifier(cell, 0.35f);
        mRedField.SetStaticModifier(cell, 0.35f);
        mRedRulesField.SetStaticModifier(cell, 0.35f);

        const int idx = 7 * kGridW + x;
        if (!mBlockedGrid[idx])
            mModifierGrid[idx] = 0.35f;
    }
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.fields_initialized"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mFieldsInitialized,
                     mFieldsInitialized ? "All 3 fields completed >= 1 Tick" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.blue_steady_state"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mBlueSteadyState,
                     mBlueSteadyState ? "Blue field peak > 0.8" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.red_steady_state"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRedSteadyState,
                     mRedSteadyState ? "Red field peak > 0.8" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.walls_respected"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mWallsRespected,
                     mWallsRespected ? "All sampled wall cells read 0.0" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.box_write_burst"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mBoxWriteBurst,
                     mBoxWriteBurst ? ">= 4 box cells exceed 0.5 after burst" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.local_maxima_found"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mLocalMaximaFound,
                     mLocalMaximaFound ? "FindLocalMaxima returned exactly 2 Blue peaks" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.contested_zone_stable"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mContestedZoneStable,
                     mContestedZoneStable ? "FindCellsAboveThreshold(0.4) on Combined >= 10" : "pending", 0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("scalarfield.gradient_non_zero_at_probe"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mGradientNonZero,
                     mGradientNonZero ? "GetGradient at (10,7) magnitude > 0.05" : "pending", 0.0f };
        });
}

// ---------------------------------------------------------------------------
// RegisterMetrics
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::RegisterMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricBluePeak)
        mMetricBluePeak = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.blue_peak"));
    if (!mMetricRedPeak)
        mMetricRedPeak = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.red_peak"));
    if (!mMetricContestedCellCount)
        mMetricContestedCellCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.contested_cell_count"));
    if (!mMetricBlueCellsAboveHalf)
        mMetricBlueCellsAboveHalf = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.blue_cells_above_half"));
    if (!mMetricRedCellsAboveHalf)
        mMetricRedCellsAboveHalf = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.red_cells_above_half"));
    if (!mMetricGradientProbeMag)
        mMetricGradientProbeMag = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.scalarfield.gradient_probe_magnitude"));
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupGrid();
    RegisterCheckpoints(service);
    RegisterMetrics();

#ifdef DIA_DEBUG
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            const Dia::Maths::Vector2D origin(0.0f, 0.0f);

            // Blue heatmap (blue→red colour ramp)
            Dia::ScalarField::Adaptors::OverlayColourMap blueMap;
            blueMap.lowColour  = Dia::Core::RGBA(0,   20, 80,  120);
            blueMap.highColour = Dia::Core::RGBA(40, 100, 255, 220);
            blueMap.minValue   = 0.0f;
            blueMap.maxValue   = 1.0f;

            mBlueHeatmap = std::make_unique<
                Dia::ScalarField::Adaptors::ScalarFieldHeatmapOverlay<
                    Dia::ScalarField::SquareFieldTopology,
                    Dia::ScalarField::UniformDecayPolicy>>(
                mBlueField,
                Dia::Debug::LayerNames::kScalarFieldHeatmap,
                kCellSize,
                origin,
                blueMap);

            // Combined heatmap (blue for negative / red for positive)
            Dia::ScalarField::Adaptors::OverlayColourMap combinedMap;
            combinedMap.lowColour  = Dia::Core::RGBA(220,  40,  40, 150);
            combinedMap.highColour = Dia::Core::RGBA( 40, 100, 220, 150);
            combinedMap.minValue   = -1.0f;
            combinedMap.maxValue   =  1.0f;

            mCombinedHeatmap = std::make_unique<
                Dia::ScalarField::Adaptors::ScalarFieldHeatmapOverlay<
                    Dia::ScalarField::SquareFieldTopology,
                    Dia::ScalarField::UniformDecayPolicy>>(
                mCombinedField,
                Dia::Core::StringCRC("scalarfield.combined_heatmap"),
                kCellSize,
                origin,
                combinedMap);

            // Combined gradient overlay
            mCombinedGradient = std::make_unique<
                Dia::ScalarField::Adaptors::ScalarFieldGradientOverlay<
                    Dia::ScalarField::SquareFieldTopology,
                    Dia::ScalarField::UniformDecayPolicy>>(
                mCombinedField,
                Dia::Debug::LayerNames::kScalarFieldGradient,
                kCellSize,
                origin,
                0.35f,
                0.01f,
                Dia::Core::RGBA(255, 255, 255, 160));

            // ImGui stats drawer
            mStatsDrawer = std::make_unique<ScalarFieldTestDrawer>(
                mBlueField, mRedField, mCombinedField,
                mFieldsInitialized,
                mBlueSteadyState, mRedSteadyState, mWallsRespected,
                mBoxWriteBurst, mLocalMaximaFound, mContestedZoneStable,
                mGradientNonZero, mBlueBurstFired, mRedBurstFired);

            Dia::Core::StringCRC stageTag("ScalarField");
            vd->GetLayerManager().Register(mBlueHeatmap.get(),      1, stageTag);
            vd->GetLayerManager().Register(mCombinedHeatmap.get(),  2, stageTag);
            vd->GetLayerManager().Register(mCombinedGradient.get(), 3, stageTag);
            vd->GetLayerManager().Register(mStatsDrawer.get(),      4, stageTag);
        }
    }
#endif

    DIA_LOG_INFO("CluicheTest", "ScalarFieldTestStageModule::OnStart — grid configured");
}

// ---------------------------------------------------------------------------
// TickFields — write sources, tick, combine
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::TickFields()
{
    // Blue sources: (2,2) and (2,12)
    mBlueField.WriteRadial(Dia::ScalarField::CellIndex{2, 2},  2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);
    mBlueField.WriteRadial(Dia::ScalarField::CellIndex{2, 12}, 2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);

    // Red sources: (17,2) and (17,12)
    mRedField.WriteRadial(Dia::ScalarField::CellIndex{17, 2},  2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);
    mRedField.WriteRadial(Dia::ScalarField::CellIndex{17, 12}, 2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);

    // Red rules field mirrors Red sources
    mRedRulesField.WriteRadial(Dia::ScalarField::CellIndex{17, 2},  2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);
    mRedRulesField.WriteRadial(Dia::ScalarField::CellIndex{17, 12}, 2.5f, 1.0f, Dia::ScalarField::FalloffCurve::kQuadratic);

    mBlueField.Tick();
    mRedField.Tick();
    mRedRulesField.Tick();

    // Combine into mCombinedField: blue +1.0, red -1.0
    Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::SquareScalarField::WeightedField, 32> inputs;
    inputs.Add({&mBlueField,  1.0f});
    inputs.Add({&mRedField,  -1.0f});
    Dia::ScalarField::SquareScalarField::Combine(mCombinedField, inputs);
}

// ---------------------------------------------------------------------------
// RunSpatialQueries — evaluate all 8 checkpoint booleans
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::RunSpatialQueries()
{
    const unsigned int frame = GetFrameCount();

    // fields_initialized: at least 1 tick has occurred (frame > 0 after first TickFields)
    if (!mFieldsInitialized && frame > 0)
        mFieldsInitialized = true;

    // blue_steady_state: peak > 0.8
    if (!mBlueSteadyState)
    {
        const float blueA = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 2});
        const float blueB = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 12});
        if (blueA > 0.8f || blueB > 0.8f)
            mBlueSteadyState = true;
    }

    // red_steady_state: peak > 0.8
    if (!mRedSteadyState)
    {
        const float redA = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 2});
        const float redB = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 12});
        if (redA > 0.8f || redB > 0.8f)
            mRedSteadyState = true;
    }

    // walls_respected: sample left-wall cells, all must read 0.0
    if (!mWallsRespected && mFieldsInitialized)
    {
        bool wallsOk = true;
        for (int y = 3; y <= 8 && wallsOk; ++y)
        {
            if (mBlueField.GetValue(Dia::ScalarField::CellIndex{5, y}) > 0.001f)
                wallsOk = false;
            if (mRedField.GetValue(Dia::ScalarField::CellIndex{14, y}) > 0.001f)
                wallsOk = false;
        }
        mWallsRespected = wallsOk;
    }

    // box_write_burst: after frame 60, check that >= 4 cells in box (9,6)-(11,8) exceed 0.5
    if (!mBoxWriteBurst && mBlueBurstFired)
    {
        int above = 0;
        for (int y = 6; y <= 8; ++y)
            for (int x = 9; x <= 11; ++x)
                if (mBlueField.GetValue(Dia::ScalarField::CellIndex{x, y}) > 0.5f)
                    ++above;
        if (above >= 4)
            mBoxWriteBurst = true;
    }

    // local_maxima_found: Blue field should have exactly 2 local maxima over full grid
    if (!mLocalMaximaFound && mBlueSteadyState)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> maxima;
        mBlueField.FindLocalMaxima(
            Dia::ScalarField::CellIndex{0, 0}, kGridW, kGridH, maxima);
        if (maxima.Size() == 2)
            mLocalMaximaFound = true;
    }

    // contested_zone_stable: FindCellsAboveThreshold(0.4) on Combined returns >= 10 cells
    if (!mContestedZoneStable && mBlueSteadyState && mRedSteadyState)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> contested;
        mCombinedField.FindCellsAboveThreshold(
            0.4f,
            Dia::ScalarField::CellIndex{0, 0}, kGridW, kGridH, contested);
        if (contested.Size() >= 10)
            mContestedZoneStable = true;
    }

    // gradient_non_zero_at_probe: GetGradient at (10,7) magnitude > 0.05
    if (!mGradientNonZero && mFieldsInitialized)
    {
        Dia::Maths::Vector2D grad = mCombinedField.GetGradient(Dia::ScalarField::CellIndex{10, 7});
        const float mag = std::sqrtf(grad.x * grad.x + grad.y * grad.y);
        if (mag > 0.05f)
            mGradientNonZero = true;
    }
}

// ---------------------------------------------------------------------------
// UpdateMetrics
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::UpdateMetrics()
{
    // Blue peak (max of two source cells)
    if (mMetricBluePeak)
    {
        const float a = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 2});
        const float b = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 12});
        mMetricBluePeak->Set(static_cast<double>(a > b ? a : b));
    }

    // Red peak
    if (mMetricRedPeak)
    {
        const float a = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 2});
        const float b = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 12});
        mMetricRedPeak->Set(static_cast<double>(a > b ? a : b));
    }

    // Contested cell count (combined > 0.4)
    if (mMetricContestedCellCount)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> contested;
        mCombinedField.FindCellsAboveThreshold(
            0.4f,
            Dia::ScalarField::CellIndex{0, 0}, kGridW, kGridH, contested);
        mMetricContestedCellCount->Set(static_cast<double>(contested.Size()));
    }

    // Blue cells above 0.5
    if (mMetricBlueCellsAboveHalf)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> above;
        mBlueField.FindCellsAboveThreshold(
            0.5f,
            Dia::ScalarField::CellIndex{0, 0}, kGridW, kGridH, above);
        mMetricBlueCellsAboveHalf->Set(static_cast<double>(above.Size()));
    }

    // Red cells above 0.5
    if (mMetricRedCellsAboveHalf)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> above;
        mRedField.FindCellsAboveThreshold(
            0.5f,
            Dia::ScalarField::CellIndex{0, 0}, kGridW, kGridH, above);
        mMetricRedCellsAboveHalf->Set(static_cast<double>(above.Size()));
    }

    // Gradient probe magnitude at (10,7)
    if (mMetricGradientProbeMag)
    {
        Dia::Maths::Vector2D grad = mCombinedField.GetGradient(Dia::ScalarField::CellIndex{10, 7});
        const float mag = std::sqrtf(grad.x * grad.x + grad.y * grad.y);
        mMetricGradientProbeMag->Set(static_cast<double>(mag));
    }
}

// ---------------------------------------------------------------------------
// AllCheckpointsPassed
// ---------------------------------------------------------------------------

bool ScalarFieldTestStageModule::AllCheckpointsPassed() const
{
    return mFieldsInitialized
        && mBlueSteadyState
        && mRedSteadyState
        && mWallsRespected
        && mBoxWriteBurst
        && mLocalMaximaFound
        && mContestedZoneStable
        && mGradientNonZero;
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::OnUpdate(float /*deltaTime*/)
{
    const unsigned int frame = GetFrameCount();

    // Tick fields every 2 frames — halves visible propagation speed.
    if (frame % 2 != 0)
    {
        RunSpatialQueries();
        UpdateMetrics();
        return;
    }

    // Burst events (write before Tick so burst values propagate this frame)
    if (frame == 60 && !mBlueBurstFired)
    {
        mBlueField.WriteBox(Dia::ScalarField::CellIndex{9, 6}, 3, 3, 0.8f);
        mBlueBurstFired = true;
        DIA_LOG_INFO("CluicheTest", "ScalarFieldTestStageModule: Blue burst at frame %u", frame);
    }

    if (frame == 180 && !mRedBurstFired)
    {
        mRedField.WriteBox(Dia::ScalarField::CellIndex{9, 6}, 3, 3, 0.8f);
        mRedBurstFired = true;
        DIA_LOG_INFO("CluicheTest", "ScalarFieldTestStageModule: Red burst at frame %u", frame);
    }

    // 1. Tick all fields
    TickFields();

    // 2. Evaluate checkpoint booleans
    RunSpatialQueries();

    // 3. Emit metrics
    UpdateMetrics();

    // 4. Pass condition
    if (AllCheckpointsPassed() && !mAllPassed)
    {
        mAllPassed = true;
        if (!IsResolved() && frame >= kMinDisplayFrames)
            ReportPassed();
    }
}

// ---------------------------------------------------------------------------
// OnStop — reset all mutable state
// ---------------------------------------------------------------------------

void ScalarFieldTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        if (mBlueHeatmap)
            vd->GetLayerManager().Unregister(mBlueHeatmap->GetLayerName());
        if (mCombinedHeatmap)
            vd->GetLayerManager().Unregister(mCombinedHeatmap->GetLayerName());
        if (mCombinedGradient)
            vd->GetLayerManager().Unregister(mCombinedGradient->GetLayerName());
        if (mStatsDrawer)
            vd->GetLayerManager().Unregister(mStatsDrawer->GetLayerName());
    }
    mBlueHeatmap.reset();
    mCombinedHeatmap.reset();
    mCombinedGradient.reset();
    mStatsDrawer.reset();
#endif

    mBlueBurstFired    = false;
    mRedBurstFired     = false;
    mFieldsInitialized = false;
    mBlueSteadyState   = false;
    mRedSteadyState    = false;
    mWallsRespected    = false;
    mBoxWriteBurst     = false;
    mLocalMaximaFound  = false;
    mContestedZoneStable = false;
    mGradientNonZero   = false;
    mAllPassed         = false;
}

} // namespace CluicheTest

namespace { using ScalarFieldTestStageModule_ = CluicheTest::ScalarFieldTestStageModule; }
DIA_MODULE(ScalarFieldTestStageModule_);
DIA_DESCRIBE(ScalarFieldTestStageModule_::kTypeId, "Tactical influence maps: Blue + Red faction fields, spatial queries, overlays");
