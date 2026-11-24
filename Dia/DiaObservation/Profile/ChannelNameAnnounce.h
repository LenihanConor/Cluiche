#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// POD event on the framework "$perfnames" EventStream: a (crc -> name) pair for an id that is not a module
		// instance id (e.g. a deferred-step channel), announced by whichever PU owns it so Main can resolve hex ids
		// in perf replies. Trivially copyable. Names longer than kMaxName-1 chars are truncated (always NUL terminated).
		struct ChannelNameAnnounce
		{
			static constexpr uint32_t kMaxName = 64;
			uint32_t crc;
			char     name[kMaxName];
		};
		static_assert(sizeof(ChannelNameAnnounce) == 68, "ChannelNameAnnounce wire size changed");
		static_assert(std::is_trivially_copyable<ChannelNameAnnounce>::value, "ChannelNameAnnounce must be trivially copyable");

		// Builds an announce, truncating the name to fit and NUL terminating it (null name -> empty).
		ChannelNameAnnounce MakeChannelNameAnnounce(uint32_t crc, const char* name);

		// Main-side registry of announced names. Plain (not thread safe): owned and used by one thread.
		class ChannelNameTable
		{
		public:
			static constexpr uint32_t kMaxEntries = 256;   // bounded: later new ids beyond this are ignored

			// Last write wins for a known crc. The name is re-terminated defensively. An empty name is ignored.
			void Apply(const ChannelNameAnnounce& a);

			// Null when unknown.
			const char* Find(uint32_t crc) const;

			uint32_t Count() const { return static_cast<uint32_t>(mEntries.size()); }
			uint32_t CrcAt(uint32_t i) const { return mEntries[i].first; }
			const char* NameAt(uint32_t i) const { return mEntries[i].second.c_str(); }

		private:
			std::vector<std::pair<uint32_t, std::string>> mEntries;
		};
	}}
}
