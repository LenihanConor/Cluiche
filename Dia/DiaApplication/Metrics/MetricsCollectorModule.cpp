#include "DiaApplication/Metrics/MetricsCollectorModule.h"

#include <DiaCore/Core/Log.h>

#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Dia
{
	namespace Application
	{
		MetricsSnapshot::MetricsSnapshot()
			: puCount(0)
			, memoryUsedMB(0.0f)
			, uptimeSeconds(0.0f)
		{
			memset(puMetrics, 0, sizeof(puMetrics));
		}

		const Dia::Core::StringCRC MetricsCollectorModule::kTypeId("MetricsCollectorModule");
		const float MetricsCollectorModule::kSmoothingAlpha = 0.1f;

		MetricsCollectorModule::MetricsCollectorModule(ProcessingUnit* pu)
			: Module(pu, kTypeId, Module::RunningEnum::kUpdate)
			, mUptimeAccumulator(0.0f)
		{
		}

		StateObject::OpertionResponse MetricsCollectorModule::DoStart(const IStartData* /*startData*/)
		{
			mUptimeAccumulator = 0.0f;
			mSnapshot = MetricsSnapshot();
			Dia::Core::Log::OutputVaradicLine("MetricsCollectorModule: Started");
			return StateObject::OpertionResponse::kImmediate;
		}

		void MetricsCollectorModule::DoUpdate()
		{
			QueryMemory();
		}

		void MetricsCollectorModule::DoStop()
		{
			Dia::Core::Log::OutputVaradicLine("MetricsCollectorModule: Stopped");
		}

		void MetricsCollectorModule::ReportFrame(const Dia::Core::StringCRC& puId,
												  const char* puName,
												  float deltaTimeMs)
		{
			int index = FindOrAddPU(puId, puName);
			if (index < 0)
				return;

			PUMetrics& m = mSnapshot.puMetrics[index];
			m.frameTimeMs = deltaTimeMs;

			float instantFps = (deltaTimeMs > 0.001f) ? (1000.0f / deltaTimeMs) : 0.0f;

			if (m.smoothedFps <= 0.0f)
				m.smoothedFps = instantFps;
			else
				m.smoothedFps = kSmoothingAlpha * instantFps + (1.0f - kSmoothingAlpha) * m.smoothedFps;

			m.fps = m.smoothedFps;

			mUptimeAccumulator += deltaTimeMs / 1000.0f;
			mSnapshot.uptimeSeconds = mUptimeAccumulator;
		}

		int MetricsCollectorModule::FindOrAddPU(const Dia::Core::StringCRC& puId, const char* puName)
		{
			for (unsigned int i = 0; i < mSnapshot.puCount; ++i)
			{
				if (mSnapshot.puMetrics[i].id == puId)
					return static_cast<int>(i);
			}

			if (mSnapshot.puCount >= MetricsSnapshot::kMaxProcessingUnits)
				return -1;

			unsigned int index = mSnapshot.puCount++;
			PUMetrics& m = mSnapshot.puMetrics[index];
			m.id = puId;
			m.fps = 0.0f;
			m.frameTimeMs = 0.0f;
			m.smoothedFps = 0.0f;
			strncpy_s(m.name, sizeof(m.name), puName, _TRUNCATE);
			return static_cast<int>(index);
		}

		void MetricsCollectorModule::QueryMemory()
		{
#ifdef _WIN32
			PROCESS_MEMORY_COUNTERS pmc;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
			{
				mSnapshot.memoryUsedMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
			}
#else
			mSnapshot.memoryUsedMB = 0.0f;
#endif
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _MetricsCollectorModule = Dia::Application::MetricsCollectorModule; }
DIA_REGISTER_MODULE(_MetricsCollectorModule) {
	Dia::Application::MetricsCollectorModule* mod = new Dia::Application::MetricsCollectorModule(pu);
	pu->SetMetricsCollector(mod);
	return mod;
}
