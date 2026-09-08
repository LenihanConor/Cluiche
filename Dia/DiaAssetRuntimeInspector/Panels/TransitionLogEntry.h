#pragma once

#include "DiaAssetRuntimeInspector/Panels/AssetStateRow.h"

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Inspector
		{
			enum class LogEntryType
			{
				kTransition = 0,
				kMarkerDisconnect,
				kMarkerReconnect
			};

			struct TransitionLogEntry
			{
				LogEntryType mType;
				Dia::Core::StringCRC mAssetId;
				AssetStateEnum mOldState;
				AssetStateEnum mNewState;
				unsigned long long mTimestamp;

				TransitionLogEntry()
					: mType(LogEntryType::kTransition)
					, mAssetId()
					, mOldState(AssetStateEnum::kNull)
					, mNewState(AssetStateEnum::kNull)
					, mTimestamp(0)
				{}
			};
		}
	}
}
