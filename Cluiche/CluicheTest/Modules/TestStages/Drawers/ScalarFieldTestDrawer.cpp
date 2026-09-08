#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/ScalarFieldTestDrawer.h"
#include <DiaCore/DebugDraw/IDebugDraw.h>

namespace CluicheTest {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ScalarFieldTestDrawer::ScalarFieldTestDrawer(
    const Dia::ScalarField::SquareScalarField& blueField,
    const Dia::ScalarField::SquareScalarField& redField,
    const Dia::ScalarField::SquareScalarField& combinedField,
    const bool& fieldsInitialized,
    const bool& blueSteadyState,
    const bool& redSteadyState,
    const bool& wallsRespected,
    const bool& boxWriteBurst,
    const bool& localMaximaFound,
    const bool& contestedZoneStable,
    const bool& gradientNonZero,
    const bool& blueBurstFired,
    const bool& redBurstFired)
    : mBlueField(blueField)
    , mRedField(redField)
    , mCombinedField(combinedField)
    , mFieldsInitialized(fieldsInitialized)
    , mBlueSteadyState(blueSteadyState)
    , mRedSteadyState(redSteadyState)
    , mWallsRespected(wallsRespected)
    , mBoxWriteBurst(boxWriteBurst)
    , mLocalMaximaFound(localMaximaFound)
    , mContestedZoneStable(contestedZoneStable)
    , mGradientNonZero(gradientNonZero)
    , mBlueBurstFired(blueBurstFired)
    , mRedBurstFired(redBurstFired)
{}

// ---------------------------------------------------------------------------
// GetLayerName
// ---------------------------------------------------------------------------

Dia::Core::StringCRC ScalarFieldTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("scalarfield.stats");
}

// ---------------------------------------------------------------------------
// Draw — no-op; this drawer's ImGui output was removed with the console.
// ---------------------------------------------------------------------------

void ScalarFieldTestDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

} // namespace CluicheTest

#endif // DIA_DEBUG
