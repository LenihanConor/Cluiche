// Suite: ScalarFieldCombine

#include <gtest/gtest.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/HexFieldTopology.h>
#include <DiaScalarField/CFieldTopology.h>
#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/UniformDecayPolicy.h>
#include <DiaScalarField/Testing/ScalarFieldTestHelpers.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::ScalarField;
using Dia::Maths::Vector2D;
using WeightedFieldArray = Dia::Core::Containers::DynamicArrayC<SquareScalarField::WeightedField, 32>;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// Combine tests
// ---------------------------------------------------------------------------

TEST(ScalarFieldCombine, Combine_SingleField_Weight1_CopiesValues)
{
    SquareScalarField fieldA = MakeNoDecayField(5, 5);
    fieldA.WritePoint(CellIndex{ 2, 2 }, 0.7f);
    fieldA.Tick();

    SquareScalarField result = MakeNoDecayField(5, 5);

    WeightedFieldArray inputs;
    inputs.Add(SquareScalarField::WeightedField{ &fieldA, 1.0f });

    SquareScalarField::Combine(result, inputs);

    Dia::ScalarField::Testing::AssertCellValue(result, CellIndex{ 2, 2 }, 0.7f, 1e-4f);
}

TEST(ScalarFieldCombine, Combine_NegativeWeight_Subtracts)
{
    SquareScalarField fieldA = MakeNoDecayField(5, 5);
    fieldA.WritePoint(CellIndex{ 2, 2 }, 0.8f);
    fieldA.Tick();

    // Result clamped to [0,1] by default, so 0 - 0.4 => clamped to 0
    SquareScalarField result = MakeNoDecayField(5, 5);

    WeightedFieldArray inputs;
    inputs.Add(SquareScalarField::WeightedField{ &fieldA, -0.5f });

    SquareScalarField::Combine(result, inputs);

    // -0.4 clamped to 0.0 (default clamp min is 0)
    EXPECT_LE(result.GetValue(CellIndex{ 2, 2 }), 0.0f + 1e-4f);
}

TEST(ScalarFieldCombine, Combine_TwoFields_WeightedSum_Correct)
{
    SquareScalarField fieldA = MakeNoDecayField(5, 5);
    fieldA.WritePoint(CellIndex{ 2, 2 }, 0.6f);
    fieldA.Tick();

    SquareScalarField fieldB = MakeNoDecayField(5, 5);
    fieldB.WritePoint(CellIndex{ 2, 2 }, 0.4f);
    fieldB.Tick();

    SquareScalarField result = MakeNoDecayField(5, 5);

    WeightedFieldArray inputs;
    inputs.Add(SquareScalarField::WeightedField{ &fieldA, 0.5f });
    inputs.Add(SquareScalarField::WeightedField{ &fieldB, 0.5f });

    SquareScalarField::Combine(result, inputs);

    // 0.6*0.5 + 0.4*0.5 = 0.3 + 0.2 = 0.5
    Dia::ScalarField::Testing::AssertCellValue(result, CellIndex{ 2, 2 }, 0.5f, 1e-4f);
}

TEST(ScalarFieldCombine, Combine_ClampsResult_RespectsBounds)
{
    SquareScalarField fieldA = MakeNoDecayField(5, 5);
    fieldA.WritePoint(CellIndex{ 2, 2 }, 1.0f);
    fieldA.Tick();

    // Result field with tight max clamp of 0.3
    SquareScalarField result = MakeNoDecayField(5, 5);
    result.SetClampRange(0.0f, 0.3f);

    WeightedFieldArray inputs;
    inputs.Add(SquareScalarField::WeightedField{ &fieldA, 1.0f });

    SquareScalarField::Combine(result, inputs);

    // Combined would be 1.0 but clamped to 0.3
    EXPECT_LE(result.GetValue(CellIndex{ 2, 2 }), 0.3f + 1e-4f);
}
