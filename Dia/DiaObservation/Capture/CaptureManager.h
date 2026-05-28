////////////////////////////////////////////////////////////////////////////////
// Filename: CaptureManager.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Capture/CaptureTypes.h>
#include <DiaGraphics/Interface/FrameCapture.h>
#include <DiaObservation/Health/HealthReporterBase.h>

#include <cstdint>

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Dia { namespace Observation { class SessionManager; } }
namespace Dia { namespace Observation { namespace Metric { class Counter; class Histogram; } } }

namespace Dia
{
    namespace Observation
    {
        namespace Capture
        {
            class CaptureManager
            {
            public:
                CaptureManager();
                ~CaptureManager();

                void Initialize(Dia::Graphics::ICanvas* canvas, SessionManager* session);
                void Tick();

                CaptureRequestStatus RequestCapture(const CaptureMetadata& metadata);

                unsigned int GetPendingCount() const;

            private:
                static constexpr unsigned int kMaxInFlight = 4;

                struct InFlightCapture
                {
                    Dia::Graphics::FrameCaptureToken token;
                    CaptureMetadata                  metadata;
                    uint64_t                         frameNumber;
                    uint32_t                         scenarioStepCrc;
                };

                class CaptureHealthReporter : public Dia::Observation::Health::HealthReporterBase
                {
                public:
                    explicit CaptureHealthReporter(CaptureManager& owner) : mOwner(owner) {}
                    Dia::Core::StringCRC GetReporterName() const override
                    {
                        return Dia::Core::StringCRC("CaptureManager");
                    }
                    Dia::Observation::Health::Health Report() const override;
                private:
                    CaptureManager& mOwner;
                };

                Dia::Graphics::ICanvas* mCanvas;
                SessionManager*         mSession;
                InFlightCapture         mInFlight[kMaxInFlight];
                unsigned int            mInFlightCount;
                bool                    mCapturesDirCreated;

                // Observability
                Dia::Observation::Metric::Counter*   mCapturesWrittenCounter;
                Dia::Observation::Metric::Counter*   mCapturesRejectedCounter;
                Dia::Observation::Metric::Histogram* mPngSizeHistogram;
                unsigned int                         mConsecutiveFailures;
                CaptureHealthReporter                mHealthReporter;

                void CompleteCapture(unsigned int index, const Dia::Graphics::FrameCaptureResult& result);
                void EmitCaptureRecord(const InFlightCapture& capture, const char* filename);
                bool EnsureCapturesDirectory();
                void RemoveSlot(unsigned int index);
            };
        }
    }
}
