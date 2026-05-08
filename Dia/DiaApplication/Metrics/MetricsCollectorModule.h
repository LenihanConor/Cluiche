#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Application
	{
		struct PUMetrics
		{
			Dia::Core::StringCRC id;
			char name[64];
			float fps;
			float frameTimeMs;
			float smoothedFps;
		};

		struct MetricsSnapshot
		{
			static const unsigned int kMaxProcessingUnits = 8;

			PUMetrics puMetrics[kMaxProcessingUnits];
			unsigned int puCount;
			float memoryUsedMB;
			float uptimeSeconds;

			MetricsSnapshot();
		};

		class MetricsCollectorModule : public Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			MetricsCollectorModule(ProcessingUnit* pu);

			void ReportFrame(const Dia::Core::StringCRC& puId,
							 const char* puName,
							 float deltaTimeMs);

			const MetricsSnapshot& GetSnapshot() const { return mSnapshot; }

			virtual const char* GetStateObjectType() const override { return "MetricsCollectorModule"; }

		protected:
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoUpdate() override;
			virtual void DoStop() override;

		private:
			void QueryMemory();
			int FindOrAddPU(const Dia::Core::StringCRC& puId, const char* puName);

			MetricsSnapshot mSnapshot;
			float mUptimeAccumulator;

			static const float kSmoothingAlpha;
		};
	}
}
