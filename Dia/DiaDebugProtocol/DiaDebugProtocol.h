#ifndef DIA_DEBUG_PROTOCOL_H
#define DIA_DEBUG_PROTOCOL_H

#include "DiaDebugProtocol/generated/debug_protocol.pb.h"
#include "DiaProtobuf/ProtoJsonCodec.h"
#include "DiaCore/Time/TimeAbsolute.h"

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
	}
}

#endif // DIA_DEBUG_PROTOCOL_H
