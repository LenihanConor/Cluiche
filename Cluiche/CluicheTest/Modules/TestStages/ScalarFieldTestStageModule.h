#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/Adaptors/RulesPropagationPolicy.h>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/TestStages/Drawers/ScalarFieldTestDrawer.h"
#include <DiaScalarField/Adaptors/ScalarFieldOverlay.h>
#include <memory>
#endif

namespace Dia::Observation::Metric { class Gauge; }

namespace CluicheTest {

class ScalarFieldTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Tactical influence maps: Blue + Red faction fields, spatial queries, overlays";
    static constexpr unsigned int kMinDisplayFrames = 300;

    explicit ScalarFieldTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~ScalarFieldTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 900; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    // -------------------------------------------------------------------------
    // Grid constants
    // -------------------------------------------------------------------------
    static constexpr int   kGridW    = 20;
    static constexpr int   kGridH    = 15;
    static constexpr int   kCellCount = kGridW * kGridH;  // 300
    static constexpr float kCellSize  = 32.0f;

    // -------------------------------------------------------------------------
    // RulesPropagationPolicy modifier fn for mRedRulesField.
    // blocked[] and modifier[] are set by SetupGrid() via pointers into the
    // sibling member arrays mBlockedGrid / mModifierGrid.
    // -------------------------------------------------------------------------
    struct ScalarModifierFn
    {
        const bool*  blocked  = nullptr;
        const float* modifier = nullptr;

        float operator()(int cellIndex) const
        {
            if (!blocked || !modifier)
                return 1.0f;
            if (cellIndex < 0 || cellIndex >= kCellCount)
                return 0.0f;
            if (blocked[cellIndex])
                return 0.0f;
            return modifier[cellIndex];
        }
    };

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------
    void SetupGrid();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    void RegisterMetrics();
    void TickFields();
    void RunSpatialQueries();
    void UpdateMetrics();
    bool AllCheckpointsPassed() const;

    // -------------------------------------------------------------------------
    // Modifier arrays (must be declared before mRedRulesField so pointer capture is valid)
    // -------------------------------------------------------------------------
    bool  mBlockedGrid[kCellCount]  = {};
    float mModifierGrid[kCellCount];

    // -------------------------------------------------------------------------
    // Fields
    //   mBlueField       — SquareScalarField; feeds Combine with +1.0
    //   mRedField        — SquareScalarField; feeds Combine with -1.0
    //   mRedRulesField   — RulesPropagationPolicy variant; exercises the adaptor code path
    //   mCombinedField   — SquareScalarField; written by Combine() each frame
    // -------------------------------------------------------------------------
    Dia::ScalarField::SquareScalarField mBlueField;
    Dia::ScalarField::SquareScalarField mRedField;

    Dia::ScalarField::DiaScalarField<
        Dia::ScalarField::SquareFieldTopology,
        Dia::ScalarField::Adaptors::RulesPropagationPolicy<ScalarModifierFn>>
        mRedRulesField;

    Dia::ScalarField::SquareScalarField mCombinedField;

    // -------------------------------------------------------------------------
    // Burst flags
    // -------------------------------------------------------------------------
    bool mBlueBurstFired = false;
    bool mRedBurstFired  = false;

    // -------------------------------------------------------------------------
    // Checkpoint booleans
    // -------------------------------------------------------------------------
    bool mFieldsInitialized   = false;
    bool mBlueSteadyState     = false;
    bool mRedSteadyState      = false;
    bool mWallsRespected      = false;
    bool mBoxWriteBurst       = false;
    bool mLocalMaximaFound    = false;
    bool mContestedZoneStable = false;
    bool mGradientNonZero     = false;
    bool mAllPassed           = false;

    // -------------------------------------------------------------------------
    // Metrics
    // -------------------------------------------------------------------------
    // Reusable scratch buffer for spatial queries — avoids large stack allocations.
    Dia::Core::Containers::DynamicArrayC<Dia::ScalarField::CellIndex, 1024> mCellQueryBuffer;

    Dia::Observation::Metric::Gauge* mMetricBluePeak           = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRedPeak            = nullptr;
    Dia::Observation::Metric::Gauge* mMetricContestedCellCount = nullptr;
    Dia::Observation::Metric::Gauge* mMetricBlueCellsAboveHalf = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRedCellsAboveHalf  = nullptr;
    Dia::Observation::Metric::Gauge* mMetricGradientProbeMag   = nullptr;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};

    std::unique_ptr<Dia::ScalarField::Adaptors::ScalarFieldHeatmapOverlay<
        Dia::ScalarField::SquareFieldTopology, Dia::ScalarField::UniformDecayPolicy>> mBlueHeatmap;

    std::unique_ptr<Dia::ScalarField::Adaptors::ScalarFieldHeatmapOverlay<
        Dia::ScalarField::SquareFieldTopology, Dia::ScalarField::UniformDecayPolicy>> mCombinedHeatmap;

    std::unique_ptr<Dia::ScalarField::Adaptors::ScalarFieldGradientOverlay<
        Dia::ScalarField::SquareFieldTopology, Dia::ScalarField::UniformDecayPolicy>> mCombinedGradient;

    std::unique_ptr<ScalarFieldTestDrawer> mStatsDrawer;
#endif
};

} // namespace CluicheTest
