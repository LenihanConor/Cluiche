#include "DiaMaths/Core/RandomStreamManager.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		RandomStreamManager::RandomStreamManager()
		{
			// Initialize hash table with reasonable capacity
			// Payload capacity: 16 streams, Table size: 16 buckets
			mStreams.SetSize(16, 16);
		}

		//-----------------------------------------------------------------------------
		RandomStreamManager::~RandomStreamManager()
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			// Delete all streams
			for (auto it = mStreams.Begin(); it != mStreams.End(); ++it)
			{
				delete it.Value();
			}

			mStreams.Clear();
		}

		//-----------------------------------------------------------------------------
		bool RandomStreamManager::CreateStream(Dia::Core::StringCRC name, unsigned int seed)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			// Check if stream already exists
			if (mStreams.Contains(name))
			{
				return false;  // Stream already exists
			}

			// Check if we need to grow the hash table
			if (mStreams.IsFull())
			{
				// Double the capacity
				unsigned int newCapacity = mStreams.Capacity() * 2;
				unsigned int newTableSize = mStreams.TableSize() * 2;

				// Create a temporary copy
				Dia::Core::Containers::HashTable<Dia::Core::StringCRC, RandomStream*> temp;
				temp.SetSize(newCapacity, newTableSize);

				// Copy all existing streams
				for (auto it = mStreams.Begin(); it != mStreams.End(); ++it)
				{
					temp.Add(it.GetKey(), it.Value());
				}

				// Swap
				mStreams = temp;
			}

			// Create new stream
			RandomStream* newStream = new RandomStream(seed);
			mStreams.Add(name, newStream);

			return true;
		}

		//-----------------------------------------------------------------------------
		RandomStream* RandomStreamManager::GetStream(Dia::Core::StringCRC name)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			// Try to get the stream pointer
			RandomStream** ppStream = mStreams.TryGetItem(name);
			if (ppStream == nullptr)
			{
				return nullptr;  // Not found
			}

			return *ppStream;
		}

		//-----------------------------------------------------------------------------
		bool RandomStreamManager::DestroyStream(Dia::Core::StringCRC name)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			// Check if stream exists
			if (!mStreams.Contains(name))
			{
				return false;  // Stream doesn't exist
			}

			// Get and delete the stream
			RandomStream** ppStream = mStreams.TryGetItem(name);
			if (ppStream != nullptr)
			{
				delete *ppStream;
			}

			// Remove from map
			mStreams.Remove(name);

			return true;
		}

		//-----------------------------------------------------------------------------
		bool RandomStreamManager::HasStream(Dia::Core::StringCRC name) const
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			return mStreams.Contains(name);
		}
	}
}
