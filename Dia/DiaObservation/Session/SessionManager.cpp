#include "DiaObservation/Session/SessionManager.h"
#include "DiaObservation/Session/SessionIdGenerator.h"
#include "DiaObservation/Session/ScenarioStepStack.h"
#include "DiaObservation/Log/ObservationFileSink.h"
#include "DiaObservation/Log/Logger.h"
#include "DiaObservation/Log/DiaLog.h"
#include "DiaObservation/Trace/Tracer.h"
#include "DiaObservation/Metric/MetricsFileSink.h"
#include "DiaObservation/Metric/MetricRegistry.h"
#include "DiaObservation/Health/HealthRegistry.h"

#include "DiaCore/Json/external/json/json.h"

#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia
{
	namespace Observation
	{
		SessionManager* SessionManager::sActiveInstance = nullptr;

		SessionManager::SessionManager()
			: mStarted(false)
			, mObservationFileSink(nullptr)
			, mMetricsFileSink(nullptr)
			, mMetricSnapshotAccumMs(0.0f)
			, mEpochOffsetNs(0)
			, mHealthPollAccumMs(0.0f)
			, mStartTimeUnixNano(0)
			, mFrameCount(0)
			, mErrorCount(0)
			, mWarningCount(0)
			, mRetentionRing(nullptr)
			, mRetentionWriteIndex(0)
			, mRetentionCount(0)
		{
			std::memset(mSessionId, 0, sizeof(mSessionId));
			std::memset(mSessionDir, 0, sizeof(mSessionDir));
			std::memset(&mConfig, 0, sizeof(mConfig));
			mEmergencyDumpGuard.clear();
		}

		SessionManager::~SessionManager()
		{
			if (mStarted)
				Stop();

			delete[] mRetentionRing;
			mRetentionRing = nullptr;
		}

		static Log::LogLevel MapConfigLevel(LogLevelConfig level)
		{
			switch (level)
			{
			case LogLevelConfig::kTrace:   return Log::LogLevel::kTrace;
			case LogLevelConfig::kDebug:   return Log::LogLevel::kDebug;
			case LogLevelConfig::kInfo:    return Log::LogLevel::kInfo;
			case LogLevelConfig::kWarning: return Log::LogLevel::kWarning;
			case LogLevelConfig::kError:   return Log::LogLevel::kError;
			}
			return Log::LogLevel::kInfo;
		}

		bool SessionManager::Start(const SessionConfig& config, const ObservationConfig& obsConfig)
		{
			if (mStarted)
			{
				DIA_LOG_WARNING("Observation", "SessionManager::Start called while already started");
				return false;
			}

			mConfig = config;

			Internal::GenerateSessionId(mSessionId);

			snprintf(mSessionDir, sizeof(mSessionDir), "%s/sessions/%s/",
				mConfig.outRootDir, mSessionId);

			if (!CreateSessionDirectory())
			{
				DIA_LOG_ERROR("Observation", "SessionManager: failed to create session directory: %s", mSessionDir);
				return false;
			}

			mRetentionRing = new Log::LogEntry[kRetentionCapacity];
			mRetentionWriteIndex = 0;
			mRetentionCount = 0;

			// Compute epoch offset: system_clock - steady_clock at this instant
			auto sysNow = std::chrono::system_clock::now();
			auto steadyNow = std::chrono::steady_clock::now();
			int64_t sysNanosSinceEpoch = std::chrono::duration_cast<std::chrono::nanoseconds>(
				sysNow.time_since_epoch()).count();
			int64_t steadyNanos = static_cast<int64_t>(steadyNow.time_since_epoch().count());
			int64_t epochOffsetNs = sysNanosSinceEpoch - steadyNanos;
			mEpochOffsetNs = epochOffsetNs;

			mStartTimeUnixNano = static_cast<uint64_t>(sysNanosSinceEpoch);

			// Configure Logger global level and per-channel overrides
			Log::Logger& logger = Log::Logger::Instance();
			logger.SetMinLevel(MapConfigLevel(obsConfig.globalLogLevel));
			for (unsigned int i = 0; i < obsConfig.channelOverrideCount; ++i)
			{
				logger.SetChannelOverride(
					obsConfig.channelOverrides[i].channel,
					MapConfigLevel(obsConfig.channelOverrides[i].level));
			}

			// Create and register ObservationFileSink (conditionally)
			if (obsConfig.enableObservationFileSink)
			{
				char logPath[600];
				snprintf(logPath, sizeof(logPath), "%slog.jsonl", mSessionDir);
				mObservationFileSink = new Log::ObservationFileSink(logPath, mSessionId, epochOffsetNs);

				if (mObservationFileSink->IsOpen())
				{
					logger.RegisterSink(mObservationFileSink);
				}
				else
				{
					DIA_LOG_ERROR("Observation", "SessionManager: failed to open log.jsonl at: %s", logPath);
					delete mObservationFileSink;
					mObservationFileSink = nullptr;
				}
			}

			// Create and start TraceFileSink (conditionally)
			if (obsConfig.enableTraceFileSink)
			{
				char tracePath[600];
				snprintf(tracePath, sizeof(tracePath), "%strace.jsonl", mSessionDir);
				Trace::Tracer::Instance().Start(tracePath, mSessionId, epochOffsetNs);
			}

			// Create and register MetricsFileSink (conditionally)
			if (obsConfig.enableMetricsFileSink)
			{
				char metricPath[600];
				snprintf(metricPath, sizeof(metricPath), "%smetric.jsonl", mSessionDir);
				char finalPath[600];
				snprintf(finalPath, sizeof(finalPath), "%smetrics-final.json", mSessionDir);

				mMetricsFileSink = new Metric::MetricsFileSink(
					metricPath, finalPath, mSessionId, epochOffsetNs);

				if (mMetricsFileSink->IsOpen())
				{
					Metric::MetricRegistry::Instance().RegisterSink(mMetricsFileSink);
				}
				else
				{
					DIA_LOG_ERROR("Observation", "SessionManager: failed to open metric.jsonl at: %s", metricPath);
					delete mMetricsFileSink;
					mMetricsFileSink = nullptr;
				}
			}

			mMetricSnapshotAccumMs = 0.0f;

			// Register retention callback for Warning/Error entries
			logger.SetRetentionCallback(&SessionManager::RetentionCallbackStatic, this);

			// Install terminate handler
			sActiveInstance = this;
			std::set_terminate(&SessionManager::TerminateHandler);

			mStarted = true;
			mFrameCount = 0;
			mErrorCount = 0;
			mWarningCount = 0;

			DIA_LOG_INFO("Observation", "Session started: %s", mSessionId);
			return true;
		}

		void SessionManager::Tick(float deltaTime)
		{
			// Metric snapshot on interval
			mMetricSnapshotAccumMs += deltaTime * 1000.0f;
			if (mMetricSnapshotAccumMs >= static_cast<float>(kMetricSnapshotIntervalMs))
			{
				mMetricSnapshotAccumMs -= static_cast<float>(kMetricSnapshotIntervalMs);

				Metric::MetricSnapshot snapshot;
				Metric::MetricRegistry::Instance().Snapshot(snapshot);
				snapshot.intervalMs = kMetricSnapshotIntervalMs;
				Metric::MetricRegistry::Instance().NotifySnapshot(snapshot);
			}

			// Health polling on interval
			mHealthPollAccumMs += deltaTime * 1000.0f;
			if (mHealthPollAccumMs >= kHealthPollIntervalMs)
			{
				mHealthPollAccumMs -= kHealthPollIntervalMs;
				PollAndEmitHealthTransitions();
			}
		}

		void SessionManager::Stop()
		{
			if (!mStarted)
				return;

			mStarted = false;

			// Fire final metric snapshot (notify all registered sinks, not just file sink)
			{
				Metric::MetricSnapshot finalSnapshot;
				Metric::MetricRegistry::Instance().Snapshot(finalSnapshot);
				finalSnapshot.intervalMs = kMetricSnapshotIntervalMs;
				Metric::MetricRegistry::Instance().NotifyFinal(finalSnapshot);
			}

			Trace::Tracer::Instance().Stop();

			// Write health.json from final health poll
			WriteHealthJson();

			Log::Logger::Instance().SetRetentionCallback(nullptr, nullptr);

			WriteSessionJson("normal_exit", 0);

			if (mObservationFileSink)
			{
				Log::Logger::Instance().UnregisterSink(mObservationFileSink);
				delete mObservationFileSink;
				mObservationFileSink = nullptr;
			}

			if (mMetricsFileSink)
			{
				Metric::MetricRegistry::Instance().UnregisterSink(mMetricsFileSink);
				delete mMetricsFileSink;
				mMetricsFileSink = nullptr;
			}

			sActiveInstance = nullptr;

			delete[] mRetentionRing;
			mRetentionRing = nullptr;
		}

		void SessionManager::PushScenarioStep(const Dia::Core::StringCRC& step)
		{
			ScenarioStepStack::Push(step);
		}

		void SessionManager::PopScenarioStep()
		{
			ScenarioStepStack::Pop();
		}

		Dia::Core::StringCRC SessionManager::GetCurrentScenarioStep() const
		{
			return ScenarioStepStack::Current();
		}

		void SessionManager::IncrementFrameCount()
		{
			++mFrameCount;
		}

		void SessionManager::OnRetainableEntry(const Log::LogEntry& entry)
		{
			if (entry.level != Log::LogLevel::kWarning && entry.level != Log::LogLevel::kError)
				return;

			std::lock_guard<std::mutex> lock(mRetentionMutex);

			if (entry.level == Log::LogLevel::kWarning)
				++mWarningCount;
			else if (entry.level == Log::LogLevel::kError)
				++mErrorCount;

			if (mRetentionRing)
			{
				mRetentionRing[mRetentionWriteIndex] = entry;
				mRetentionWriteIndex = (mRetentionWriteIndex + 1) % kRetentionCapacity;
				if (mRetentionCount < kRetentionCapacity)
					++mRetentionCount;
			}
		}

		void SessionManager::EmergencyDump()
		{
			if (mEmergencyDumpGuard.test_and_set())
				return;

			WriteHealthJson();
			WriteCrashJson();
			WriteSessionJson("terminate", 1);
		}

		void SessionManager::RetentionCallbackStatic(const Log::LogEntry& entry, void* userData)
		{
			static_cast<SessionManager*>(userData)->OnRetainableEntry(entry);
		}

		void SessionManager::TerminateHandler()
		{
			if (sActiveInstance)
				sActiveInstance->EmergencyDump();

			std::abort();
		}

		bool SessionManager::CreateSessionDirectory()
		{
			// Create path components one at a time
			char path[512];
			std::memset(path, 0, sizeof(path));

			for (size_t i = 0; mSessionDir[i] != '\0' && i < sizeof(path) - 1; ++i)
			{
				path[i] = mSessionDir[i];
				if ((path[i] == '/' || path[i] == '\\') && i > 2)
				{
					CreateDirectoryA(path, nullptr);
				}
			}

			return CreateDirectoryA(mSessionDir, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
		}

		void SessionManager::PollAndEmitHealthTransitions()
		{
			Health::HealthRegistry::Transition transitions[32];
			unsigned int transitionCount = 0;
			Health::HealthRegistry::Instance().PollTransitions(transitions, 32, transitionCount);

			for (unsigned int i = 0; i < transitionCount; ++i)
			{
				const Health::HealthRegistry::Transition& t = transitions[i];
				DIA_LOG_INFO("health", "Health transition: %s %s -> %s (reason: %s)",
					t.reporterName.AsChar(),
					Health::HealthStatusToString(t.oldStatus),
					Health::HealthStatusToString(t.newStatus),
					t.reason.AsChar() ? t.reason.AsChar() : "");
			}
		}

		void SessionManager::WriteHealthJson()
		{
			Health::HealthRegistry::ReporterSnapshot reporters[32];
			unsigned int reporterCount = 0;
			Health::HealthRegistry::Instance().Snapshot(reporters, 32, reporterCount);

			// Determine overall status
			Health::HealthStatus overall = Health::HealthStatus::kOK;
			for (unsigned int i = 0; i < reporterCount; ++i)
			{
				if (reporters[i].health.status == Health::HealthStatus::kFailing)
				{
					overall = Health::HealthStatus::kFailing;
					break;
				}
				if (reporters[i].health.status == Health::HealthStatus::kDegraded)
					overall = Health::HealthStatus::kDegraded;
			}

			Json::Value root;
			root["schema_version"] = "1.0";
			root["session_id"] = mSessionId;
			root["overall_status"] = Health::HealthStatusToString(overall);

			Json::Value modules(Json::arrayValue);
			for (unsigned int i = 0; i < reporterCount; ++i)
			{
				Json::Value mod;
				mod["name"] = reporters[i].name.AsChar() ? reporters[i].name.AsChar() : "";
				mod["status"] = Health::HealthStatusToString(reporters[i].health.status);
				mod["errors"] = reporters[i].health.errors;
				mod["warnings"] = reporters[i].health.warnings;
				mod["reason"] = reporters[i].health.reason.AsChar()
					? reporters[i].health.reason.AsChar() : "";
				modules.append(mod);
			}
			root["modules"] = modules;

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			std::string jsonStr = Json::writeString(builder, root);

			char healthPath[600];
			snprintf(healthPath, sizeof(healthPath), "%shealth.json", mSessionDir);

			FILE* f = nullptr;
			fopen_s(&f, healthPath, "wb");
			if (f)
			{
				fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
				fclose(f);
			}
		}

		void SessionManager::WriteSessionJson(const char* exitReason, int exitCode)
		{
			auto sysNow = std::chrono::system_clock::now();
			uint64_t endTimeUnixNano = static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					sysNow.time_since_epoch()).count());

			uint64_t durationMs = (endTimeUnixNano - mStartTimeUnixNano) / 1000000ULL;

			Json::Value root;
			root["schema_version"] = "1.0";

			Json::Value& session = root["session"];
			session["id"] = mSessionId;
			session["app_name"] = mConfig.appName;
			session["build_version"] = mConfig.buildVersion;
			session["build_config"] = mConfig.buildConfig;
			session["platform"] = "win64";
			session["started_unix_nano"] = Json::Value(static_cast<Json::Int64>(mStartTimeUnixNano));
			session["ended_unix_nano"] = Json::Value(static_cast<Json::Int64>(endTimeUnixNano));
			session["duration_ms"] = Json::Value(static_cast<Json::UInt64>(durationMs));

			Json::Value& exit = root["exit"];
			exit["reason"] = exitReason;
			exit["code"] = exitCode;
			exit["stage_at_exit"] = "";

			Json::Value& counts = root["counts"];
			counts["errors"] = mErrorCount;
			counts["warnings"] = mWarningCount;
			counts["frames"] = Json::Value(static_cast<Json::UInt64>(mFrameCount));
			counts["scenario_steps_completed"] = 0;

			root["scenario_steps"] = Json::Value(Json::arrayValue);

			// Populate modules from HealthRegistry
			Json::Value modulesArr(Json::arrayValue);
			{
				Health::HealthRegistry::ReporterSnapshot reporters[32];
				unsigned int reporterCount = 0;
				Health::HealthRegistry::Instance().Snapshot(reporters, 32, reporterCount);
				for (unsigned int i = 0; i < reporterCount; ++i)
				{
					Json::Value mod;
					mod["name"] = reporters[i].name.AsChar() ? reporters[i].name.AsChar() : "";
					mod["status"] = Health::HealthStatusToString(reporters[i].health.status);
					mod["errors"] = reporters[i].health.errors;
					mod["warnings"] = reporters[i].health.warnings;
					modulesArr.append(mod);
				}
			}
			root["modules"] = modulesArr;

			// Retained warnings/errors
			Json::Value retained(Json::arrayValue);
			{
				std::lock_guard<std::mutex> lock(mRetentionMutex);
				if (mRetentionRing && mRetentionCount > 0)
				{
					unsigned int startIdx = (mRetentionCount >= kRetentionCapacity)
						? mRetentionWriteIndex : 0;
					for (unsigned int i = 0; i < mRetentionCount; ++i)
					{
						unsigned int idx = (startIdx + i) % kRetentionCapacity;
						const Log::LogEntry& e = mRetentionRing[idx];
						Json::Value rec;
						rec["level"] = Log::LogLevelToString(e.level);
						rec["channel"] = e.channel.AsChar();
						rec["msg"] = e.message;
						retained.append(rec);
					}
				}
			}
			root["retained_warnings_and_errors"] = retained;

			// Files array — scan session directory
			Json::Value files(Json::arrayValue);
			{
				char searchPath[600];
				snprintf(searchPath, sizeof(searchPath), "%s*", mSessionDir);

				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA(searchPath, &findData);
				if (hFind != INVALID_HANDLE_VALUE)
				{
					do
					{
						if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
							continue;

						Json::Value fileEntry;
						fileEntry["path"] = findData.cFileName;

						const char* ext = strrchr(findData.cFileName, '.');
						if (ext && strcmp(ext, ".jsonl") == 0)
							fileEntry["kind"] = "log";
						else
							fileEntry["kind"] = "data";

						LARGE_INTEGER fileSize;
						fileSize.LowPart = findData.nFileSizeLow;
						fileSize.HighPart = findData.nFileSizeHigh;
						fileEntry["bytes"] = Json::Value(static_cast<Json::Int64>(fileSize.QuadPart));

						files.append(fileEntry);
					}
					while (FindNextFileA(hFind, &findData));
					FindClose(hFind);
				}
			}
			root["files"] = files;

			// Write via temp-file-and-rename for atomicity (clean stop only)
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			std::string jsonStr = Json::writeString(builder, root);

			char tmpPath[600];
			snprintf(tmpPath, sizeof(tmpPath), "%ssession.json.tmp", mSessionDir);

			char finalPath[600];
			snprintf(finalPath, sizeof(finalPath), "%ssession.json", mSessionDir);

			FILE* f = nullptr;
			fopen_s(&f, tmpPath, "wb");
			if (f)
			{
				fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
				fclose(f);
				MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING);
			}
			else
			{
				// Fallback: write directly (crash path or permission issue)
				fopen_s(&f, finalPath, "wb");
				if (f)
				{
					fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
					fclose(f);
				}
			}
		}

		void SessionManager::WriteCrashJson()
		{
			// Create crashes/ subdirectory
			char crashDir[600];
			snprintf(crashDir, sizeof(crashDir), "%scrashes/", mSessionDir);
			CreateDirectoryA(crashDir, nullptr);

			char crashPath[600];
			snprintf(crashPath, sizeof(crashPath), "%s0.json", crashDir);

			Json::Value root;
			root["schema_version"] = "1.0";

			Json::Value& session = root["session"];
			session["id"] = mSessionId;
			session["app_name"] = mConfig.appName;

			Json::Value& exit = root["exit"];
			exit["reason"] = "terminate";

			Json::Value retained(Json::arrayValue);
			{
				// No mutex in crash path — we may be in an inconsistent state,
				// but best-effort is better than nothing.
				if (mRetentionRing && mRetentionCount > 0)
				{
					unsigned int startIdx = (mRetentionCount >= kRetentionCapacity)
						? mRetentionWriteIndex : 0;
					for (unsigned int i = 0; i < mRetentionCount; ++i)
					{
						unsigned int idx = (startIdx + i) % kRetentionCapacity;
						const Log::LogEntry& e = mRetentionRing[idx];
						Json::Value rec;
						rec["level"] = Log::LogLevelToString(e.level);
						rec["channel"] = e.channel.AsChar();
						rec["msg"] = e.message;
						retained.append(rec);
					}
				}
			}
			root["retained_warnings_and_errors"] = retained;
			root["open_log_entries_at_crash"] = Json::Value(Json::arrayValue);

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			std::string jsonStr = Json::writeString(builder, root);

			// Direct write — no temp-rename in crash context
			FILE* f = nullptr;
			fopen_s(&f, crashPath, "wb");
			if (f)
			{
				fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
				fclose(f);
			}
		}
	}
} // namespace Dia
