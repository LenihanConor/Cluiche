#include "DiaLogger/ISink.h"

namespace Dia
{
	namespace Logger
	{
		ISink::ISink()
			: mMinLevel(LogLevel::kInfo)
		{
		}

		void ISink::SetLevelThreshold(LogLevel minLevel)
		{
			mMinLevel = minLevel;
		}

		LogLevel ISink::GetLevelThreshold() const
		{
			return mMinLevel;
		}

		void ISink::SetChannelFilter(const Dia::Core::StringCRC& channel, bool enabled)
		{
			bool found = false;
			for (unsigned int i = 0; i < mChannelWhitelist.Size(); ++i)
			{
				if (mChannelWhitelist[i] == channel)
				{
					found = true;
					if (!enabled)
					{
						mChannelWhitelist.RemoveAt(i);
					}
					break;
				}
			}

			if (enabled && !found && !mChannelWhitelist.IsFull())
			{
				mChannelWhitelist.Add(channel);
			}
		}

		void ISink::ClearChannelFilter()
		{
			mChannelWhitelist.RemoveAll();
		}

		bool ISink::AcceptsEntry(const LogEntry& entry) const
		{
			if (static_cast<unsigned char>(entry.level) < static_cast<unsigned char>(mMinLevel))
				return false;

			if (mChannelWhitelist.Size() == 0)
				return true;

			for (unsigned int i = 0; i < mChannelWhitelist.Size(); ++i)
			{
				if (mChannelWhitelist[i] == entry.channel)
					return true;
			}

			return false;
		}
	}
}
