#ifndef DIA_DEBUG_PROTOCOL_H
#define DIA_DEBUG_PROTOCOL_H

#include "DiaDebugProtocol/generated/debug_protocol.pb.h"
#include "DiaProtobuf/ProtoJsonCodec.h"
#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaApplication/Metrics/MetricsCollectorModule.h"

#include <cstdint>

namespace Dia
{
	namespace DebugProtocol
	{
		static constexpr int kProtocolVersion = 1;

		inline uint64_t GetTimestampNow()
		{
			return static_cast<uint64_t>(Dia::Core::TimeAbsolute::GetSystemTime().AsLongLongInMicroseconds());
		}

		inline void PopulateCoreMetrics(dia::debug::CoreMetrics* cm, const Dia::Application::MetricsSnapshot& snap)
		{
			if (snap.puCount > 0)
			{
				cm->set_fps(snap.puMetrics[0].fps);
				cm->set_frame_time_ms(snap.puMetrics[0].frameTimeMs);
			}
			cm->set_memory_used_mb(snap.memoryUsedMB);
			cm->set_uptime_seconds(snap.uptimeSeconds);
			for (unsigned int i = 0; i < snap.puCount && i < Dia::Application::MetricsSnapshot::kMaxProcessingUnits; ++i)
			{
				auto* pu = cm->add_processing_units();
				pu->set_name(snap.puMetrics[i].name);
				pu->set_fps(snap.puMetrics[i].fps);
				pu->set_frame_time_ms(snap.puMetrics[i].frameTimeMs);
			}
		}
	}
}

#endif // DIA_DEBUG_PROTOCOL_H
