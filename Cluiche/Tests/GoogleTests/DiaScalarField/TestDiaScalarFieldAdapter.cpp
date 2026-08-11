// Suite: ScalarFieldAdapter
// Tests for DiaScalarFieldAdapter<Topology, Policy> against IDiaScalarField.

#include <gtest/gtest.h>

#include <DiaScalarField/IDiaScalarField.h>
#include <DiaScalarField/DiaScalarFieldAdapter.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/CellIndex.h>
#include <DiaScalarField/UniformDecayPolicy.h>

using namespace Dia::ScalarField;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// No-decay policy: values set via WritePoint + Tick() are preserved exactly,
// with no diffusion from neighbours, so assertions stay simple.
static UniformDecayPolicy NoDecay()
{
    return UniformDecayPolicy(UniformDecayParams{ 0.0f, 0.0f });
}

static SquareScalarField MakeField(int w, int h)
{
    return SquareScalarField(
        SquareFieldTopology(w, h, SquareConnectivity::k4Connected),
        NoDecay());
}

// ---------------------------------------------------------------------------
// 1. GetName() returns the name set at construction
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, GetName_ReturnsConstructedName)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "danger");

    EXPECT_STREQ(adapter.GetName(), "danger");
}

TEST(ScalarFieldAdapter, GetName_ViaInterface_ReturnsConstructedName)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "influence");

    const IDiaScalarField& iface = adapter;
    EXPECT_STREQ(iface.GetName(), "influence");
}

// ---------------------------------------------------------------------------
// 2. GetCellCount() returns the correct cell count
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, GetCellCount_MatchesTopology_3x3)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    EXPECT_EQ(adapter.GetCellCount(), 9);
}

TEST(ScalarFieldAdapter, GetCellCount_MatchesTopology_5x4)
{
    auto field = MakeField(5, 4);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    EXPECT_EQ(adapter.GetCellCount(), 20);
}

// ---------------------------------------------------------------------------
// 3. CaptureSnapshot() + GetSnapshotCount() increments up to ring size
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, CaptureSnapshot_IncreasesCount_WhenBelowRingSize)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", /*historyDepth=*/5);

    EXPECT_EQ(adapter.GetSnapshotCount(), 0);

    adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), 1);

    adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), 2);

    adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), 3);
}

TEST(ScalarFieldAdapter, CaptureSnapshot_CountCapsAtHistoryDepth)
{
    auto field = MakeField(3, 3);
    const int depth = 3;
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", depth);

    // Fill the ring.
    for (int i = 0; i < depth; ++i)
        adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), depth);

    // Extra captures must not grow the count beyond historyDepth.
    adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), depth);

    adapter.CaptureSnapshot();
    EXPECT_EQ(adapter.GetSnapshotCount(), depth);
}

// ---------------------------------------------------------------------------
// 4. Ring indexing: oldest at frame=0, most recent at frame=snapshotCount-1
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, GetSnapshotValue_BeforeRingFull_Frame0IsOldest)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", /*historyDepth=*/10);

    const CellIndex cell{ 1, 1 };

    // Snapshot 0: value 0.1
    field.WritePoint(cell, 0.1f);
    field.Tick();
    adapter.CaptureSnapshot();

    // Snapshot 1: value 0.2
    field.WritePoint(cell, 0.2f);
    field.Tick();
    adapter.CaptureSnapshot();

    // Snapshot 2: value 0.3
    field.WritePoint(cell, 0.3f);
    field.Tick();
    adapter.CaptureSnapshot();

    // Ring not yet full — frame=0 is the oldest (first captured).
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(0, cell), 0.1f);
    // frame=2 is the most recent.
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(2, cell), 0.3f);
}

TEST(ScalarFieldAdapter, GetSnapshotValue_WhenRingSaturated_Frame0IsOldest)
{
    auto field = MakeField(3, 3);
    const int depth = 3;
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", depth);

    const CellIndex cell{ 0, 0 };

    // Fill ring: snapshots with values 0.1, 0.2, 0.3.
    for (int i = 1; i <= depth; ++i)
    {
        field.WritePoint(cell, static_cast<float>(i) * 0.1f);
        field.Tick();
        adapter.CaptureSnapshot();
    }
    ASSERT_EQ(adapter.GetSnapshotCount(), depth);

    // Overwrite oldest with 0.4 — ring wraps.
    field.WritePoint(cell, 0.4f);
    field.Tick();
    adapter.CaptureSnapshot();

    // Now ring holds [0.2, 0.3, 0.4].
    // frame=0 = oldest = 0.2
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(0, cell), 0.2f);
    // frame=2 = most recent = 0.4
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(depth - 1, cell), 0.4f);
}

TEST(ScalarFieldAdapter, GetSnapshotValue_OutOfRange_ReturnsZero)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", /*historyDepth=*/5);

    const CellIndex cell{ 0, 0 };

    // No snapshots yet.
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(0, cell), 0.0f);

    adapter.CaptureSnapshot();

    // frame=-1 and frame=1 are out of range.
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(-1, cell), 0.0f);
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(1,  cell), 0.0f);
}

// ---------------------------------------------------------------------------
// 5. WritePoint sets a cell value; verify via GetValue
//    (WritePoint queues the write; Tick() flushes it to the read buffer)
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, WritePoint_ThenTick_GetValueReflectsWrite)
{
    auto field = MakeField(4, 4);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    const CellIndex cell{ 2, 2 };
    const float     value = 0.75f;

    adapter.WritePoint(cell, value);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue(cell), value);
}

TEST(ScalarFieldAdapter, WritePoint_DifferentCells_OnlyTargetCellChanged)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    const CellIndex target{ 1, 1 };
    const CellIndex other { 0, 0 };

    adapter.WritePoint(target, 0.5f);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue(target), 0.5f);
    // With zero diffusion and zero decay, untouched cells remain 0.
    EXPECT_FLOAT_EQ(adapter.GetValue(other), 0.0f);
}

// ---------------------------------------------------------------------------
// 6. Adapter correctly wraps a real DiaScalarField instance
//    (identity: adapter delegates to the same underlying field object)
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, GetValue_ReflectsUnderlyingField_AfterDirectFieldTick)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    const CellIndex cell{ 0, 2 };

    // Write directly on the field, tick the field.
    field.WritePoint(cell, 0.9f);
    field.Tick();

    // Adapter must see the same value as the underlying field.
    EXPECT_FLOAT_EQ(adapter.GetValue(cell), field.GetValue(cell));
    EXPECT_FLOAT_EQ(adapter.GetValue(cell), 0.9f);
}

TEST(ScalarFieldAdapter, CaptureSnapshot_StoresFieldStateAtCaptureTime)
{
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f", /*historyDepth=*/10);

    const CellIndex cell{ 1, 0 };

    // State A: value 0.6
    field.WritePoint(cell, 0.6f);
    field.Tick();
    adapter.CaptureSnapshot();

    // Mutate the field after capturing.
    field.WritePoint(cell, 0.0f);
    field.Tick();

    // Snapshot 0 must still contain 0.6 (snapshot is immutable after capture).
    EXPECT_FLOAT_EQ(adapter.GetSnapshotValue(0, cell), 0.6f);
    // Live value is now 0.0.
    EXPECT_FLOAT_EQ(adapter.GetValue(cell), 0.0f);
}

// ---------------------------------------------------------------------------
// 7. WriteRadial — adapter delegates to the underlying field
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, WriteRadial_CenterCellGetsPeakValue_Linear)
{
    // 5x5 grid, radial at (2,2) r=2 peak=1 linear.
    // Centre cell: dist=0, attenuation=1.0 → value=1.0.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    const CellIndex center{ 2, 2 };
    adapter.WriteRadial(center, 2.0f, 1.0f, FalloffCurve::kLinear);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue(center), 1.0f);
}

TEST(ScalarFieldAdapter, WriteRadial_CellBeyondRadius_StaysZero_Linear)
{
    // 5x5 grid, radial at (2,2) r=1 peak=1 linear.
    // Cell (0,0): dist = sqrt(8) ≈ 2.83 > 1 → not touched → stays 0.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteRadial({2, 2}, 1.0f, 1.0f, FalloffCurve::kLinear);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({0, 0}), 0.0f);
}

TEST(ScalarFieldAdapter, WriteRadial_IntermediateCell_LinearAttenuation)
{
    // 5x5 grid, radial at (2,2) r=2 peak=1 linear.
    // Cell (2,1): dist=1, attenuation = 1 - 1/2 = 0.5 → value=0.5.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteRadial({2, 2}, 2.0f, 1.0f, FalloffCurve::kLinear);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({2, 1}), 0.5f);
}

TEST(ScalarFieldAdapter, WriteRadial_IntermediateCell_QuadraticAttenuation)
{
    // 5x5 grid, radial at (2,2) r=2 peak=1 quadratic.
    // Cell (2,1): dist=1, t=0.5, attenuation = 1 - 0.25 = 0.75 → value=0.75.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteRadial({2, 2}, 2.0f, 1.0f, FalloffCurve::kQuadratic);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({2, 1}), 0.75f);
}

TEST(ScalarFieldAdapter, WriteRadial_ViaInterface_DelegatesToField)
{
    // Access through IDiaScalarField* to confirm interface dispatch.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");
    IDiaScalarField* iface = &adapter;

    iface->WriteRadial({2, 2}, 2.0f, 0.8f, FalloffCurve::kLinear);
    field.Tick();

    // Centre cell: dist=0 → attenuation=1.0 → 0.8 * 1.0 = 0.8.
    EXPECT_FLOAT_EQ(adapter.GetValue({2, 2}), 0.8f);
}

// ---------------------------------------------------------------------------
// 8. WriteBox — adapter delegates to the underlying field
// ---------------------------------------------------------------------------

TEST(ScalarFieldAdapter, WriteBox_AllCellsInBoxGetValue)
{
    // 5x5 grid, box at (1,1) w=3 h=2.
    // Expected to hit: (1,1),(2,1),(3,1),(1,2),(2,2),(3,2) → 6 cells.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteBox({1, 1}, 3, 2, 0.7f);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({1, 1}), 0.7f);
    EXPECT_FLOAT_EQ(adapter.GetValue({2, 1}), 0.7f);
    EXPECT_FLOAT_EQ(adapter.GetValue({3, 1}), 0.7f);
    EXPECT_FLOAT_EQ(adapter.GetValue({1, 2}), 0.7f);
    EXPECT_FLOAT_EQ(adapter.GetValue({2, 2}), 0.7f);
    EXPECT_FLOAT_EQ(adapter.GetValue({3, 2}), 0.7f);
}

TEST(ScalarFieldAdapter, WriteBox_CellsOutsideBox_StayZero)
{
    // 5x5 grid, box at (1,1) w=3 h=3. Corners (0,0) and (4,4) untouched.
    auto field = MakeField(5, 5);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteBox({1, 1}, 3, 3, 0.5f);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({0, 0}), 0.0f);
    EXPECT_FLOAT_EQ(adapter.GetValue({4, 4}), 0.0f);
    EXPECT_FLOAT_EQ(adapter.GetValue({4, 0}), 0.0f);
    EXPECT_FLOAT_EQ(adapter.GetValue({0, 4}), 0.0f);
}

TEST(ScalarFieldAdapter, WriteBox_1x1_OnlyTargetCellChanged)
{
    // A 1x1 box is equivalent to WritePoint.
    auto field = MakeField(3, 3);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");

    adapter.WriteBox({1, 1}, 1, 1, 0.9f);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({1, 1}), 0.9f);
    EXPECT_FLOAT_EQ(adapter.GetValue({0, 0}), 0.0f);
    EXPECT_FLOAT_EQ(adapter.GetValue({2, 2}), 0.0f);
}

TEST(ScalarFieldAdapter, WriteBox_ViaInterface_DelegatesToField)
{
    auto field = MakeField(4, 4);
    DiaScalarFieldAdapter<SquareFieldTopology> adapter(field, "f");
    IDiaScalarField* iface = &adapter;

    iface->WriteBox({0, 0}, 2, 2, 0.6f);
    field.Tick();

    EXPECT_FLOAT_EQ(adapter.GetValue({0, 0}), 0.6f);
    EXPECT_FLOAT_EQ(adapter.GetValue({1, 1}), 0.6f);
    EXPECT_FLOAT_EQ(adapter.GetValue({2, 2}), 0.0f);
}
