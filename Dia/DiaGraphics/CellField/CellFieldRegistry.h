////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRegistry.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>
#include <DiaGraphics/CellField/CellGridDesc.h>

#include <stdint.h>

namespace Dia
{
	namespace Graphics
	{
		enum class CellFieldValueKind : uint8_t { Category, Float };

		/// Result of a registry call. error is a static string (null when ok).
		struct CellFieldResult
		{
			bool        ok = false;
			const char* error = nullptr;
		};

		/// Observation counters (snapshot).
		struct CellFieldStats
		{
			uint32_t layersAllocated = 0;	// slots in use (includes destroyed-but-not-yet-drained)
			uint32_t layersLive = 0;		// slots whose handle currently resolves
			uint32_t commandsQueued = 0;	// commands currently waiting for Drain
			uint64_t queuedBytes = 0;		// bytes currently waiting for Drain
			uint64_t creates = 0;			// accepted Create commands (lifetime)
			uint64_t updates = 0;			// accepted Update commands (lifetime)
			uint64_t destroys = 0;			// accepted Destroy commands (lifetime)
			uint64_t uploads = 0;			// Update commands drained
			uint64_t uploadedBytes = 0;		// value bytes drained
			uint64_t geometryBuilds = 0;	// Create commands drained
			uint64_t droppedCommands = 0;	// commands dropped by the byte cap
			uint64_t staleHandles = 0;		// stale handles skipped
			uint64_t rejectedRequests = 0;	// requests rejected with an error result
		};

		/// Render-thread consumer of drained commands. Pointers are valid only during the callback.
		class ICellFieldCommandSink
		{
		public:
			virtual ~ICellFieldCommandSink() {}
			virtual void OnCreate(Dia::Core::CellFieldLayerHandle handle, const CellGridDesc& ownedDesc, CellFieldValueKind kind) = 0;
			virtual void OnUpdate(Dia::Core::CellFieldLayerHandle handle, const float* values, uint32_t count) = 0;
			virtual void OnDestroy(Dia::Core::CellFieldLayerHandle handle) = 0;
		};

		///
		/// CellFieldRegistry - thread-safe, backend-agnostic layer table + command queue.
		/// Commands are copied at submit time. Destroy is deferred: the handle stops resolving
		/// immediately, but the slot stays allocated (generation unchanged) until Drain processes
		/// the Destroy; only then is the slot recycled with a bumped generation.
		///
		class CellFieldRegistry
		{
		public:
			static const uint32_t kMaxLayers = 32;
			static const uint32_t kMaxGridDim = 128;
			static const uint64_t kMaxQueueBytes = 8ull * 1024ull * 1024ull;

			static const char* const kErrMaxLayers;
			static const char* const kErrGridTooLarge;
			static const char* const kErrInvalidGrid;
			static const char* const kErrCountMismatch;
			static const char* const kErrStaleHandle;
			static const char* const kErrQueueFull;

			CellFieldRegistry();
			~CellFieldRegistry();
			CellFieldRegistry(const CellFieldRegistry&) = delete;
			CellFieldRegistry& operator=(const CellFieldRegistry&) = delete;

			CellFieldResult CreateLayer(const CellGridDesc& desc, CellFieldValueKind kind, Dia::Core::CellFieldLayerHandle& out);
			CellFieldResult Update(Dia::Core::CellFieldLayerHandle handle, const float* values, uint32_t count);
			void            Destroy(Dia::Core::CellFieldLayerHandle handle);

			/// True if the handle refers to a live layer. Increments staleHandles when it does not.
			bool ResolveHandle(Dia::Core::CellFieldLayerHandle handle);

			/// Render thread: swap the queue out under the lock, then deliver in submit order unlocked.
			/// Destroyed slots are freed (generation bumped) after their OnDestroy.
			void Drain(ICellFieldCommandSink& sink);

			CellFieldStats GetStats() const;

		private:
			struct Impl;
			Impl* mImpl;
		};
	}
}
