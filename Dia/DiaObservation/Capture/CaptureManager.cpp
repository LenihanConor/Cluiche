////////////////////////////////////////////////////////////////////////////////
// Filename: CaptureManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaObservation/Capture/CaptureManager.h"

#define LODEPNG_NO_COMPILE_DECODER
#include "DiaObservation/Capture/ThirdParty/lodepng.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Session/SessionManager.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Health/HealthRegistry.h>

#include <cstdio>
#include <cstring>
#include <windows.h>

namespace Dia
{
    namespace Observation
    {
        namespace Capture
        {
            CaptureManager::CaptureManager()
                : mCaptureSource(nullptr)
                , mSession(nullptr)
                , mPendingCount(0)
                , mInFlightCount(0)
                , mCapturesDirCreated(false)
                , mCapturesWrittenCounter(nullptr)
                , mCapturesRejectedCounter(nullptr)
                , mPngSizeHistogram(nullptr)
                , mConsecutiveFailures(0)
                , mHealthReporter(*this)
            {
                memset(mPending,  0, sizeof(mPending));
                memset(mInFlight, 0, sizeof(mInFlight));
            }

            CaptureManager::~CaptureManager()
            {
                Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);
            }

            void CaptureManager::SetCaptureSource(IFrameCaptureSource* captureSource)
            {
                mCaptureSource = captureSource;
            }

            void CaptureManager::Initialize(IFrameCaptureSource* captureSource, SessionManager* session)
            {
                mCaptureSource = captureSource;
                mSession = session;
                mInFlightCount = 0;
                mCapturesDirCreated = false;
                memset(mInFlight, 0, sizeof(mInFlight));

                auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
                mCapturesWrittenCounter  = reg.RegisterCounter(Dia::Core::StringCRC("dia.capture.written"));
                mCapturesRejectedCounter = reg.RegisterCounter(Dia::Core::StringCRC("dia.capture.rejected"));

                static const float kPngBuckets[] = { 10000.f, 50000.f, 100000.f, 500000.f, 1000000.f };
                mPngSizeHistogram = reg.RegisterHistogram(
                    Dia::Core::StringCRC("dia.capture.png_size_bytes"),
                    kPngBuckets, 5);

                mConsecutiveFailures = 0;

                Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealthReporter);
            }

            CaptureRequestStatus CaptureManager::RequestCapture(const CaptureMetadata& metadata)
            {
                if (mCaptureSource == nullptr)
                {
                    if (mCapturesRejectedCounter) mCapturesRejectedCounter->Inc();
                    return CaptureRequestStatus::kRejected_NoCanvas;
                }

                if (mSession == nullptr || !mSession->IsStarted())
                {
                    if (mCapturesRejectedCounter) mCapturesRejectedCounter->Inc();
                    return CaptureRequestStatus::kRejected_NoSession;
                }

                // Enqueue metadata; RenderTick() issues the bgfx call on the render thread.
                std::lock_guard<std::mutex> lock(mPendingMutex);
                if (mPendingCount >= kMaxPending)
                {
                    if (mCapturesRejectedCounter) mCapturesRejectedCounter->Inc();
                    return CaptureRequestStatus::kRejected_RingFull;
                }

                mPending[mPendingCount++] = metadata;
                return CaptureRequestStatus::kAccepted;
            }

            void CaptureManager::RenderTick()
            {
                // Drain pending queue — must be called from the render thread (bgfx API thread).
                unsigned int count = 0;
                CaptureMetadata local[kMaxPending];
                {
                    std::lock_guard<std::mutex> lock(mPendingMutex);
                    count = mPendingCount;
                    for (unsigned int i = 0; i < count; ++i)
                        local[i] = mPending[i];
                    mPendingCount = 0;
                }

                for (unsigned int i = 0; i < count; ++i)
                {
                    if (mInFlightCount >= kMaxInFlight)
                    {
                        if (mCapturesRejectedCounter) mCapturesRejectedCounter->Inc();
                        continue;
                    }

                    Dia::Graphics::FrameCaptureToken token = mCaptureSource->RequestFrameCapture();
                    if (!token.IsValid())
                    {
                        if (mCapturesRejectedCounter) mCapturesRejectedCounter->Inc();
                        continue;
                    }

                    InFlightCapture& slot = mInFlight[mInFlightCount];
                    slot.token           = token;
                    slot.metadata        = local[i];
                    slot.frameNumber     = mSession->GetFrameCount();
                    slot.scenarioStepCrc = mSession->GetCurrentScenarioStep().Value();
                    ++mInFlightCount;
                }
            }

            void CaptureManager::Tick()
            {
                if (mInFlightCount == 0) return;  // skip trace when nothing in flight
                DIA_TRACE_ZONE("capture.tick", ::Dia::Observation::Trace::Category::kDiaGraphics);
                for (unsigned int i = 0; i < mInFlightCount; )
                {
                    Dia::Graphics::FrameCaptureResult result = mCaptureSource->PollFrameCapture(mInFlight[i].token);

                    switch (result.status)
                    {
                    case Dia::Graphics::FrameCaptureResult::Status::kReady:
                        CompleteCapture(i, result);
                        // CompleteCapture calls RemoveSlot which swaps last→i and decrements mInFlightCount
                        // Do NOT increment i — slot i now holds what was the last slot
                        break;

                    case Dia::Graphics::FrameCaptureResult::Status::kFailed:
                        DIA_LOG_WARNING("Capture", "Frame capture failed for token %u (frame %llu)",
                            mInFlight[i].token.id,
                            static_cast<unsigned long long>(mInFlight[i].frameNumber));
                        ++mConsecutiveFailures;
                        RemoveSlot(i);
                        break;

                    case Dia::Graphics::FrameCaptureResult::Status::kInvalidToken:
                        DIA_LOG_WARNING("Capture", "Invalid token %u encountered during Tick (frame %llu)",
                            mInFlight[i].token.id,
                            static_cast<unsigned long long>(mInFlight[i].frameNumber));
                        ++mConsecutiveFailures;
                        RemoveSlot(i);
                        break;

                    case Dia::Graphics::FrameCaptureResult::Status::kPending:
                    default:
                        ++i;
                        break;
                    }
                }
            }

            unsigned int CaptureManager::GetPendingCount() const
            {
                return mInFlightCount;
            }

            bool CaptureManager::EnsureCapturesDirectory()
            {
                if (mCapturesDirCreated)
                    return true;

                char path[640];
                snprintf(path, sizeof(path), "%s/captures", mSession->GetSessionDirectory());

                BOOL result = CreateDirectoryA(path, nullptr);
                if (result == FALSE)
                {
                    DWORD err = GetLastError();
                    if (err != ERROR_ALREADY_EXISTS)
                    {
                        DIA_LOG_ERROR("Capture", "Failed to create captures directory '%s' (error %lu)", path, err);
                        return false;
                    }
                }

                mCapturesDirCreated = true;
                return true;
            }

            void CaptureManager::CompleteCapture(unsigned int index, const Dia::Graphics::FrameCaptureResult& result)
            {
                const InFlightCapture& capture = mInFlight[index];

                // Build filename: frameNumber + tag CRC in hex
                char filename[128];
                snprintf(filename, sizeof(filename), "%06llu_%08x.png",
                    static_cast<unsigned long long>(capture.frameNumber),
                    static_cast<unsigned int>(capture.metadata.tag.Value()));

                if (!EnsureCapturesDirectory())
                {
                    DIA_LOG_ERROR("Capture", "Skipping capture '%s' — could not create captures directory", filename);
                    RemoveSlot(index);
                    return;
                }

                // Build full output path
                char fullPath[768];
                snprintf(fullPath, sizeof(fullPath), "%s/captures/%s",
                    mSession->GetSessionDirectory(), filename);

                // Encode RGBA8 → PNG via lodepng
                unsigned char* pngData = nullptr;
                size_t pngSize = 0;
                DIA_PROFILE_SCOPE("capture.encode_and_write", ::Dia::Observation::Profile::Category::kDiaGraphics);
                unsigned encodeErr = lodepng_encode32(
                    &pngData,
                    &pngSize,
                    static_cast<const unsigned char*>(result.data),
                    result.width,
                    result.height);

                if (encodeErr != 0 || pngData == nullptr)
                {
                    DIA_LOG_ERROR("Capture", "lodepng_encode32 failed (error %u) for capture '%s'", encodeErr, filename);
                    if (pngData)
                        free(pngData);
                    RemoveSlot(index);
                    return;
                }

                // Write PNG to disk
                FILE* f = nullptr;
                fopen_s(&f, fullPath, "wb");
                if (f != nullptr)
                {
                    fwrite(pngData, 1, pngSize, f);
                    fclose(f);
                    DIA_LOG_INFO("Capture", "Wrote capture '%s' (%ux%u, %zu bytes)", filename, result.width, result.height, pngSize);
                    if (mCapturesWrittenCounter) mCapturesWrittenCounter->Inc();
                    if (mPngSizeHistogram)       mPngSizeHistogram->Observe(static_cast<double>(pngSize));
                    mConsecutiveFailures = 0;

                    // Also write a tag-named copy to the well-known run directory so
                    // dia check render-diff can find it by stage name without digging
                    // through session sub-directories.
                    // Path: <sessionDir>/../../captures/run/<tag>.png
                    char runDir[768];
                    snprintf(runDir, sizeof(runDir), "%s/../../captures/run", mSession->GetSessionDirectory());
                    CreateDirectoryA(runDir, nullptr); // ok if already exists
                    char runPath[896];
                    snprintf(runPath, sizeof(runPath), "%s/%s.png", runDir, capture.metadata.tag.AsChar());
                    FILE* rf = nullptr;
                    fopen_s(&rf, runPath, "wb");
                    if (rf != nullptr)
                    {
                        fwrite(pngData, 1, pngSize, rf);
                        fclose(rf);
                        DIA_LOG_INFO("Capture", "Wrote run capture '%s.png'", capture.metadata.tag.AsChar());
                    }
                    else
                    {
                        DIA_LOG_WARNING("Capture", "Could not write run capture '%s'", runPath);
                    }
                }
                else
                {
                    DIA_LOG_ERROR("Capture", "Failed to open '%s' for writing", fullPath);
                }

                free(pngData);

                EmitCaptureRecord(capture, filename);
                RemoveSlot(index);
            }

            void CaptureManager::EmitCaptureRecord(const InFlightCapture& capture, const char* filename)
            {
                // Determine trigger string
                const char* triggerStr = "code";
                switch (capture.metadata.trigger)
                {
                case TriggerSource::kManual:     triggerStr = "manual";     break;
                case TriggerSource::kAutomation: triggerStr = "automation"; break;
                case TriggerSource::kCode:       triggerStr = "code";       break;
                }

                // Build path to capture.jsonl in session directory
                char recordPath[640];
                snprintf(recordPath, sizeof(recordPath), "%s/capture.jsonl", mSession->GetSessionDirectory());

                FILE* f = nullptr;
                fopen_s(&f, recordPath, "ab");
                if (f == nullptr)
                {
                    DIA_LOG_ERROR("Capture", "Failed to open '%s' for appending capture record", recordPath);
                    return;
                }

                char line[1024];
                int len = snprintf(line, sizeof(line),
                    "{\"record_type\":\"capture\","
                    "\"schema_version\":\"1.0\","
                    "\"frame_number\":%llu,"
                    "\"filename\":\"%s\","
                    "\"tag_crc\":\"%08x\","
                    "\"tag_name\":\"%.64s\","
                    "\"context\":\"%.128s\","
                    "\"trigger\":\"%s\","
                    "\"scenario_step_crc\":\"%08x\"}\n",
                    static_cast<unsigned long long>(capture.frameNumber),
                    filename,
                    static_cast<unsigned int>(capture.metadata.tag.Value()),
                    capture.metadata.tag.AsChar(),
                    capture.metadata.context,
                    triggerStr,
                    static_cast<unsigned int>(capture.scenarioStepCrc));

                if (len > 0)
                    fwrite(line, 1, static_cast<size_t>(len), f);

                fclose(f);
            }

            void CaptureManager::RemoveSlot(unsigned int index)
            {
                if (mInFlightCount == 0)
                    return;

                unsigned int last = mInFlightCount - 1;
                if (index != last)
                    mInFlight[index] = mInFlight[last];

                memset(static_cast<void*>(&mInFlight[last]), 0, sizeof(InFlightCapture));
                --mInFlightCount;
            }

            Dia::Observation::Health::Health CaptureManager::CaptureHealthReporter::Report() const
            {
                Dia::Observation::Health::Health h;
                h.errors   = 0;
                h.warnings = 0;
                h.reason   = Dia::Core::StringCRC{};
                h.status   = Dia::Observation::Health::HealthStatus::kOK;

                if (mOwner.mCaptureSource == nullptr)
                {
                    h.status = Dia::Observation::Health::HealthStatus::kDegraded;
                    h.reason = Dia::Core::StringCRC("no_canvas");
                    ++h.warnings;
                }
                else if (mOwner.mConsecutiveFailures >= 3)
                {
                    h.status = Dia::Observation::Health::HealthStatus::kFailing;
                    h.reason = Dia::Core::StringCRC("consecutive_failures");
                    h.errors = mOwner.mConsecutiveFailures;
                }

                return h;
            }
        }
    }
}
