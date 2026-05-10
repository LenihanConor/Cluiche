#ifndef DIA_DEBUG_PROTOCOL_H
#define DIA_DEBUG_PROTOCOL_H

#pragma warning(push)
#pragma warning(disable: 4244 4267)
#include "DiaDebugProtocol/generated/debug_protocol.pb.h"
#pragma warning(pop)
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
