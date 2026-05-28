////////////////////////////////////////////////////////////////////////////////
// Filename: TestCaptureManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaObservation/Capture/CaptureManager.h>
#include <DiaObservation/Capture/CaptureTypes.h>
#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Metric/Testing/MetricFixture.h>
#include <DiaObservation/Testing/HealthFixture.h>
#include <DiaGraphics/Interface/ICanvas.h>

#include <cstring>

using namespace Dia::Observation;
using namespace Dia::Observation::Capture;

// ---------------------------------------------------------------------------
// Minimal ICanvas mock — no real rendering, just capture plumbing
// ---------------------------------------------------------------------------
namespace
{
    class MockCanvas : public Dia::Graphics::ICanvas
    {
    public:
        // Pure virtual stubs
        void Initialize(const Dia::Graphics::ICanvas::Settings&) override {}
        void SetCanvasSize(const Dia::Maths::Vector2D&) override {}
        void StartFrame(const Dia::Graphics::FrameData&) override {}
        void ProcessFrame(const Dia::Graphics::FrameData&) override {}
        void EndFrame(const Dia::Graphics::FrameData&) override {}

        // Capture control
        int  requestCount    = 0;
        bool returnValidToken = true;
        bool returnReady     = false;
        Dia::Graphics::FrameCaptureResult pendingResult;
        Dia::Graphics::FrameCaptureResult readyResult;

        Dia::Graphics::FrameCaptureToken RequestFrameCapture() override
        {
            ++requestCount;
            if (!returnValidToken)
                return Dia::Graphics::FrameCaptureToken{};  // id == 0 → invalid
            Dia::Graphics::FrameCaptureToken t;
            t.id = static_cast<unsigned int>(requestCount);  // non-zero → valid
            return t;
        }

        Dia::Graphics::FrameCaptureResult PollFrameCapture(const Dia::Graphics::FrameCaptureToken&) override
        {
            return returnReady ? readyResult : pendingResult;
        }
    };
}

// ---------------------------------------------------------------------------
// Fixture — resets registries so tests are isolated
// ---------------------------------------------------------------------------
struct CaptureManagerTest : ::testing::Test
{
    Metric::MetricFixture  metricFixture;
    Testing::HealthFixture healthFixture;

    MockCanvas canvas;

    // A started SessionManager (writes to temp dir ".")
    SessionManager session;

    ObservationConfig MakeMinimalObs()
    {
        ObservationConfig obs;
        std::memset(&obs, 0, sizeof(obs));
        obs.enableObservationFileSink = false;
        obs.enableTraceFileSink       = false;
        obs.enableMetricsFileSink     = false;
        return obs;
    }

    SessionConfig MakeConfig()
    {
        SessionConfig c;
        std::memset(&c, 0, sizeof(c));
        strncpy_s(c.appName,       "capture_test", sizeof(c.appName) - 1);
        strncpy_s(c.buildVersion,  "1.0",          sizeof(c.buildVersion) - 1);
        strncpy_s(c.buildConfig,   "debug",        sizeof(c.buildConfig) - 1);
        strncpy_s(c.outRootDir,    ".",            sizeof(c.outRootDir) - 1);
        return c;
    }

    void TearDown() override
    {
        if (session.IsStarted())
            session.Stop();
    }

    CaptureMetadata MakeMetadata()
    {
        CaptureMetadata m;
        m.tag     = Dia::Core::StringCRC("test_tag");
        m.trigger = TriggerSource::kCode;
        std::memset(m.context, 0, sizeof(m.context));
        return m;
    }
};

// ---------------------------------------------------------------------------
// T6-1  No canvas → kRejected_NoCanvas
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, RequestCapture_NoCanvas_ReturnsRejected)
{
    CaptureManager mgr;
    mgr.Initialize(nullptr, &session);

    CaptureRequestStatus status = mgr.RequestCapture(MakeMetadata());
    EXPECT_EQ(status, CaptureRequestStatus::kRejected_NoCanvas);
}

// ---------------------------------------------------------------------------
// T6-2  No session → kRejected_NoSession
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, RequestCapture_NoSession_ReturnsRejected)
{
    CaptureManager mgr;
    mgr.Initialize(&canvas, nullptr);

    CaptureRequestStatus status = mgr.RequestCapture(MakeMetadata());
    EXPECT_EQ(status, CaptureRequestStatus::kRejected_NoSession);
}

// ---------------------------------------------------------------------------
// T6-3  Session present but not started → kRejected_NoSession
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, RequestCapture_SessionNotStarted_ReturnsRejected)
{
    // session is constructed but Start() has not been called
    CaptureManager mgr;
    mgr.Initialize(&canvas, &session);

    CaptureRequestStatus status = mgr.RequestCapture(MakeMetadata());
    EXPECT_EQ(status, CaptureRequestStatus::kRejected_NoSession);
}

// ---------------------------------------------------------------------------
// T6-4  Canvas returns invalid token → kRejected_RingFull
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, RequestCapture_InvalidToken_ReturnsRingFull)
{
    session.Start(MakeConfig(), MakeMinimalObs());

    canvas.returnValidToken = false;

    CaptureManager mgr;
    mgr.Initialize(&canvas, &session);

    CaptureRequestStatus status = mgr.RequestCapture(MakeMetadata());
    EXPECT_EQ(status, CaptureRequestStatus::kRejected_RingFull);
}

// ---------------------------------------------------------------------------
// T6-5  Happy path → kAccepted + count increments
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, RequestCapture_Accepted_IncrementsCount)
{
    session.Start(MakeConfig(), MakeMinimalObs());

    // pendingResult defaults to kInvalidToken status — Tick won't complete it
    canvas.pendingResult.status = Dia::Graphics::FrameCaptureResult::Status::kPending;

    CaptureManager mgr;
    mgr.Initialize(&canvas, &session);

    CaptureRequestStatus status = mgr.RequestCapture(MakeMetadata());
    EXPECT_EQ(status, CaptureRequestStatus::kAccepted);
    EXPECT_EQ(mgr.GetPendingCount(), 1u);
}

// ---------------------------------------------------------------------------
// T6-6  Tick with kPending result keeps in-flight count unchanged
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, Tick_PendingResult_KeepsInFlight)
{
    session.Start(MakeConfig(), MakeMinimalObs());

    canvas.pendingResult.status = Dia::Graphics::FrameCaptureResult::Status::kPending;

    CaptureManager mgr;
    mgr.Initialize(&canvas, &session);

    mgr.RequestCapture(MakeMetadata());
    ASSERT_EQ(mgr.GetPendingCount(), 1u);

    mgr.Tick();
    EXPECT_EQ(mgr.GetPendingCount(), 1u);
}

// ---------------------------------------------------------------------------
// T6-7  Tick with kFailed result removes from in-flight
// ---------------------------------------------------------------------------
TEST_F(CaptureManagerTest, Tick_FailedResult_RemovesFromFlight)
{
    session.Start(MakeConfig(), MakeMinimalObs());

    canvas.pendingResult.status = Dia::Graphics::FrameCaptureResult::Status::kPending;

    CaptureManager mgr;
    mgr.Initialize(&canvas, &session);

    mgr.RequestCapture(MakeMetadata());
    ASSERT_EQ(mgr.GetPendingCount(), 1u);

    // Switch poll response to kFailed so next Tick removes the slot
    canvas.returnReady = false;
    canvas.pendingResult.status = Dia::Graphics::FrameCaptureResult::Status::kFailed;

    mgr.Tick();
    EXPECT_EQ(mgr.GetPendingCount(), 0u);
}
