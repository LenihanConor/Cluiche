// Suite: ScalarFieldObservation

#include <gtest/gtest.h>
#include <DiaScalarField/DiaScalarField.h>
#include <DiaScalarField/SquareFieldTopology.h>
#include <DiaScalarField/UniformDecayPolicy.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::ScalarField;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// Helper: 5x5 no-decay field
// ---------------------------------------------------------------------------

static SquareScalarField MakeNoDecayField(int w, int h)
{
    SquareFieldTopology topo(w, h, SquareConnectivity::k8Connected);
    UniformDecayPolicy policy(UniformDecayParams{ 0.0f, 0.0f });
    return SquareScalarField(topo, policy);
}

// ---------------------------------------------------------------------------
// ScalarFieldObservation tests
// ---------------------------------------------------------------------------

// Calling Tick() without registering metrics first must not crash.
TEST(ScalarFieldObservation, Tick_DoesNotCrash_WithNoMetrics)
{
    SquareScalarField field = MakeNoDecayField(3, 3);
    EXPECT_NO_FATAL_FAILURE(field.Tick());
}

// RegisterMetrics with valid names must not crash.
TEST(ScalarFieldObservation, RegisterMetrics_DoesNotCrash)
{
    SquareScalarField field = MakeNoDecayField(3, 3);
    EXPECT_NO_FATAL_FAILURE(
        field.RegisterMetrics(
            StringCRC("test.sf.ticks.nocr"),
            StringCRC("test.sf.cells.nocr")));
}

// After RegisterMetrics, Tick() must increment the tick counter by exactly 1.
TEST(ScalarFieldObservation, Tick_IncrementsTickCounter_AfterRegisterMetrics)
{
    SquareScalarField field = MakeNoDecayField(3, 3);
    field.RegisterMetrics(
        StringCRC("test.sf.ticks.inc"),
        StringCRC("test.sf.cells.inc"));

    auto* counter = Dia::Observation::Metric::MetricRegistry::Instance()
                        .FindCounter(StringCRC("test.sf.ticks.inc"));
    ASSERT_NE(counter, nullptr);

    const uint64_t before = counter->Value();
    field.Tick();
    const uint64_t after = counter->Value();

    EXPECT_EQ(after, before + 1u);
}

// After RegisterMetrics + Tick(), the cells gauge must equal the field's cell count.
TEST(ScalarFieldObservation, Tick_UpdatesCellsGauge_AfterRegisterMetrics)
{
    SquareScalarField field = MakeNoDecayField(3, 3); // 9 cells
    field.RegisterMetrics(
        StringCRC("test.sf.ticks.gauge"),
        StringCRC("test.sf.cells.gauge"));

    field.Tick();

    auto* gauge = Dia::Observation::Metric::MetricRegistry::Instance()
                      .FindGauge(StringCRC("test.sf.cells.gauge"));
    ASSERT_NE(gauge, nullptr);

    EXPECT_DOUBLE_EQ(gauge->Value(), 9.0);
}

// Two independent fields with different metric names must accumulate separately.
TEST(ScalarFieldObservation, MultipleFields_SeparateMetrics_NoCollision)
{
    SquareScalarField fieldA = MakeNoDecayField(3, 3);
    SquareScalarField fieldB = MakeNoDecayField(3, 3);

    fieldA.RegisterMetrics(
        StringCRC("test.sf.ticks.a"),
        StringCRC("test.sf.cells.a"));
    fieldB.RegisterMetrics(
        StringCRC("test.sf.ticks.b"),
        StringCRC("test.sf.cells.b"));

    // Tick A three times, B once.
    fieldA.Tick();
    fieldA.Tick();
    fieldA.Tick();
    fieldB.Tick();

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    auto* counterA = reg.FindCounter(StringCRC("test.sf.ticks.a"));
    auto* counterB = reg.FindCounter(StringCRC("test.sf.ticks.b"));

    ASSERT_NE(counterA, nullptr);
    ASSERT_NE(counterB, nullptr);

    EXPECT_EQ(counterA->Value(), 3u);
    EXPECT_EQ(counterB->Value(), 1u);
}
