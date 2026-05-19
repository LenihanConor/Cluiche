#pragma once

#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Observation { namespace Log
	{
		class ISink
		{
		public:
			virtual ~ISink() {}
			virtual void OnLogEntry(const LogEntry& entry) = 0;
			virtual const char* GetName() const = 0;

			void SetLevelThreshold(LogLevel minLevel);
			LogLevel GetLevelThreshold() const;

			void SetChannelFilter(const Dia::Core::StringCRC& channel, bool enabled);
			void ClearChannelFilter();
			bool AcceptsEntry(const LogEntry& entry) const;

		protected:
			ISink();

		private:
			LogLevel mMinLevel;

			static const unsigned int kMaxChannelFilters = 16;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxChannelFilters> mChannelWhitelist;
		};
	}
} // namespace Observation
} // namespace Dia
