#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaScalarField/DiaScalarField.h>

namespace CluicheTest {

// ---------------------------------------------------------------------------
// ScalarFieldTestDrawer
//
// ImGui stats panel for the ScalarField test stage.
// The heatmap and gradient overlays are separate IVisualDebugger instances
// (ScalarFieldHeatmapOverlay / ScalarFieldGradientOverlay) registered
// directly with the layer manager.  This drawer's Draw() is a no-op.
// ---------------------------------------------------------------------------
class ScalarFieldTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    ScalarFieldTestDrawer(
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
        const bool& redBurstFired);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::ScalarField::SquareScalarField& mBlueField;
    const Dia::ScalarField::SquareScalarField& mRedField;
    const Dia::ScalarField::SquareScalarField& mCombinedField;

    const bool&         mFieldsInitialized;
    const bool&         mBlueSteadyState;
    const bool&         mRedSteadyState;
    const bool&         mWallsRespected;
    const bool&         mBoxWriteBurst;
    const bool&         mLocalMaximaFound;
    const bool&         mContestedZoneStable;
    const bool&         mGradientNonZero;
    const bool&         mBlueBurstFired;
    const bool&         mRedBurstFired;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
