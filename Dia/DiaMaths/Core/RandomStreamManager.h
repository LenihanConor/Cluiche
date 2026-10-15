#pragma once

#include <DiaMaths/Core/RandomStream.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <mutex>

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// RandomStreamManager - Named Random Stream Registry
		//
		// Instance-based manager for multiple named RandomStream instances.
		// No singleton - applications can create multiple managers (e.g., one per thread/module).
		//
		// Thread Safety: Registry operations (create/get/destroy) are thread-safe via mRegistryMutex.
		//                Multiple threads can safely access different managers or the same manager's registry operations.
		//                Once a stream pointer is retrieved, the stream itself is thread-safe (per-stream mutex).
		//
		// Ownership: Manager owns all streams. Destructor deletes all streams.
		//
		// Usage:
		//   RandomStreamManager mgr;
		//   mgr.CreateStream(StringCRC("world"), 12345);
		//   RandomStream* stream = mgr.GetStream(StringCRC("world"));
		//   float value = stream->RandomUnit();
		//---------------------------------------------------------------------------------------------------------------------------------

		class RandomStreamManager
		{
		public:
			// Construct an empty manager
			RandomStreamManager();

			// Destructor - deletes all streams
			~RandomStreamManager();

			// Create a new named stream with the given seed
			// Returns true if created, false if a stream with that name already exists
			// Thread-safe: can be called concurrently from multiple threads
			bool CreateStream(Dia::Core::StringCRC name, unsigned int seed);

			// Get a stream by name
			// Returns pointer to the stream, or nullptr if it doesn't exist
			// Thread-safe: can be called concurrently from multiple threads
			// Note: The returned pointer remains valid until DestroyStream() is called for that name
			RandomStream* GetStream(Dia::Core::StringCRC name);

			// Destroy a stream by name
			// Returns true if destroyed, false if the stream didn't exist
			// Thread-safe: can be called concurrently from multiple threads
			bool DestroyStream(Dia::Core::StringCRC name);

			// Check if a stream exists
			// Returns true if a stream with that name exists, false otherwise
			// Thread-safe: can be called concurrently from multiple threads
			bool HasStream(Dia::Core::StringCRC name) const;

		private:
			// No copying
			RandomStreamManager(const RandomStreamManager&) = delete;
			RandomStreamManager& operator=(const RandomStreamManager&) = delete;

			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, RandomStream*> mStreams;
			mutable std::mutex mRegistryMutex;  // Protects mStreams map operations
		};
	}
}
