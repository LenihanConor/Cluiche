#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/ScalarFieldTestDrawer.h"
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaScalarField/CellIndex.h>
#include <imgui.h>
#include <cmath>

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
// Draw — no geometry; this drawer exists only for ImGui
// ---------------------------------------------------------------------------

void ScalarFieldTestDrawer::Draw(Dia::Core::IDebugDraw& /*draw*/)
{
}

// ---------------------------------------------------------------------------
// DrawImGui — stats panel
// ---------------------------------------------------------------------------

void ScalarFieldTestDrawer::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 260), ImGuiCond_Always);
    ImGui::Begin("Scalar Field Stage", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Blue burst: %s   Red burst: %s",
        mBlueBurstFired ? "FIRED" : "--",
        mRedBurstFired  ? "FIRED" : "--");

    ImGui::Separator();

    // Field peak values
    const float bluePeakA = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 2});
    const float bluePeakB = mBlueField.GetValue(Dia::ScalarField::CellIndex{2, 12});
    const float redPeakA  = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 2});
    const float redPeakB  = mRedField.GetValue(Dia::ScalarField::CellIndex{17, 12});

    ImGui::Text("Blue A(2,2)=%.3f  B(2,12)=%.3f", bluePeakA, bluePeakB);
    ImGui::Text("Red  A(17,2)=%.3f  B(17,12)=%.3f", redPeakA, redPeakB);

    // Gradient probe
    Dia::Maths::Vector2D grad = mCombinedField.GetGradient(Dia::ScalarField::CellIndex{10, 7});
    const float mag = std::sqrtf(grad.x * grad.x + grad.y * grad.y);
    ImGui::Text("Gradient probe (10,7) mag=%.3f", mag);

    ImGui::Separator();

    // Checkpoint list
    auto CheckRow = [](const char* name, bool passed) {
        ImGui::TextColored(
            passed ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.9f, 0.8f, 0.2f, 1.0f),
            "%s  %s", passed ? "[PASS]" : "[ -- ]", name);
    };

    CheckRow("fields_initialized",         mFieldsInitialized);
    CheckRow("blue_steady_state",          mBlueSteadyState);
    CheckRow("red_steady_state",           mRedSteadyState);
    CheckRow("walls_respected",            mWallsRespected);
    CheckRow("box_write_burst",            mBoxWriteBurst);
    CheckRow("local_maxima_found",         mLocalMaximaFound);
    CheckRow("contested_zone_stable",      mContestedZoneStable);
    CheckRow("gradient_non_zero_at_probe", mGradientNonZero);

    ImGui::End();
}

} // namespace CluicheTest

#endif // DIA_DEBUG
