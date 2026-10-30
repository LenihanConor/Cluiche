#ifndef DIA_DEBUG_SERVER_SIM_PENDING_TABLE_H
#define DIA_DEBUG_SERVER_SIM_PENDING_TABLE_H

#include <cstdint>

namespace Dia
{
	namespace DebugServer
	{
		// Tracks in-flight sim-hosted requests awaiting a reply from the SimPU.
		// Main-thread only: no locking. Time is passed in so tests can use a fake clock.
		class SimPendingTable
		{
		public:
			static constexpr uint32_t kCapacity = 64;
			static constexpr uint32_t kTimeoutMs = 2000;

			struct Entry
			{
				uint32_t requestId = 0;
				int connectionId = 0;
				// The debug protocol has no per-request id: CommandRequest/CommandResponse are
				// correlated by command name (DebugServer::HandleCommand sets resp->command to
				// the request's command string). commandId is the StringCRC value of that name.
				uint32_t commandId = 0;
				uint64_t deadlineMs = 0;
			};

			SimPendingTable();

			// Returns a fresh nonzero requestId, or 0 if the table is full.
			uint32_t Add(int connectionId, uint32_t commandId, uint64_t nowMs);

			// Removes and returns the entry. False if unknown (late or duplicate reply).
			bool Complete(uint32_t requestId, Entry& out);

			// Removes entries with deadline <= nowMs into out (up to outCapacity); returns count.
			uint32_t Expire(uint64_t nowMs, Entry* out, uint32_t outCapacity);

			void DropConnection(int connectionId);

			uint32_t Count() const { return mCount; }

			// Test hook: forces the next candidate id (to exercise wrap-around).
			void SetNextIdForTesting(uint32_t nextId) { mNextId = nextId; }

		private:
			bool IsLive(uint32_t requestId) const;

			Entry mEntries[kCapacity];
			bool mUsed[kCapacity];
			uint32_t mCount;
			uint32_t mNextId;
		};
	}
}

#endif
