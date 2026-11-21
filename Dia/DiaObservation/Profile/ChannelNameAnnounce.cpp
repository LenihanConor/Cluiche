#include <DiaObservation/Profile/ChannelNameAnnounce.h>

#include <cstring>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		ChannelNameAnnounce MakeChannelNameAnnounce(uint32_t crc, const char* name)
		{
			ChannelNameAnnounce a;
			std::memset(&a, 0, sizeof(a));
			a.crc = crc;
			if (name != nullptr)
			{
				std::strncpy(a.name, name, ChannelNameAnnounce::kMaxName - 1);
			}
			a.name[ChannelNameAnnounce::kMaxName - 1] = '\0';
			return a;
		}

		void ChannelNameTable::Apply(const ChannelNameAnnounce& a)
		{
			// Bounded scan: never trust the producer to have terminated the buffer.
			uint32_t len = 0;
			while (len < ChannelNameAnnounce::kMaxName && a.name[len] != '\0') { ++len; }
			if (len == 0) { return; }
			const std::string name(a.name, len);
			for (auto& e : mEntries)
			{
				if (e.first == a.crc) { e.second = name; return; }
			}
			if (mEntries.size() >= kMaxEntries) { return; }
			mEntries.emplace_back(a.crc, name);
		}

		const char* ChannelNameTable::Find(uint32_t crc) const
		{
			for (const auto& e : mEntries)
			{
				if (e.first == crc) { return e.second.c_str(); }
			}
			return nullptr;
		}
	}}
}
