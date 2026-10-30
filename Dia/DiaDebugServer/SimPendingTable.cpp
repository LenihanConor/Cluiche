#include <DiaDebugServer/SimPendingTable.h>

namespace Dia
{
	namespace DebugServer
	{
		SimPendingTable::SimPendingTable()
			: mUsed{}
			, mCount(0)
			, mNextId(1)
		{
		}

		bool SimPendingTable::IsLive(uint32_t requestId) const
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (mUsed[i] && mEntries[i].requestId == requestId)
					return true;
			}
			return false;
		}

		uint32_t SimPendingTable::Add(int connectionId, uint32_t commandId, uint64_t nowMs, const char* commandName)
		{
			if (mCount >= kCapacity)
				return 0;

			// Fewer than kCapacity ids are live, so this loop terminates quickly.
			uint32_t id = mNextId;
			while (id == 0 || IsLive(id))
				++id;
			mNextId = id + 1;

			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (!mUsed[i])
				{
					mUsed[i] = true;
					mEntries[i].requestId = id;
					mEntries[i].connectionId = connectionId;
					mEntries[i].commandId = commandId;
					mEntries[i].deadlineMs = nowMs + kTimeoutMs;
					mEntries[i].commandName[0] = '\0';
					if (commandName != nullptr)
					{
						strncpy(mEntries[i].commandName, commandName, kNameBytes - 1);
						mEntries[i].commandName[kNameBytes - 1] = '\0';
					}
					++mCount;
					return id;
				}
			}
			return 0;
		}

		bool SimPendingTable::Complete(uint32_t requestId, Entry& out)
		{
			if (requestId == 0)
				return false;
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (mUsed[i] && mEntries[i].requestId == requestId)
				{
					out = mEntries[i];
					mUsed[i] = false;
					--mCount;
					return true;
				}
			}
			return false;
		}

		uint32_t SimPendingTable::Expire(uint64_t nowMs, Entry* out, uint32_t outCapacity)
		{
			uint32_t n = 0;
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (mUsed[i] && mEntries[i].deadlineMs <= nowMs)
				{
					if (out != nullptr && n < outCapacity)
						out[n] = mEntries[i];
					++n;
					mUsed[i] = false;
					--mCount;
				}
			}
			return n;
		}

		void SimPendingTable::DropConnection(int connectionId)
		{
			for (uint32_t i = 0; i < kCapacity; ++i)
			{
				if (mUsed[i] && mEntries[i].connectionId == connectionId)
				{
					mUsed[i] = false;
					--mCount;
				}
			}
		}
	}
}
