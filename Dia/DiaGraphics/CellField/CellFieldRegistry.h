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

		/// One colour stop of a ramp. position in [0,1], rgba8 packed 0xRRGGBBAA.
			struct CellFieldRampStop
			{
				float    position = 0.0f;
				uint32_t rgba8 = 0;
			};

			/// Copy of a registered palette. revision increments each time the id is re-registered.
			struct CellFieldPaletteData
			{
				uint32_t count = 0;
				uint32_t colours[64] = {};
				uint32_t fallback = 0;
				uint32_t revision = 0;
			};

			/// Copy of a registered ramp. revision increments each time the id is re-registered.
			struct CellFieldRampData
			{
				uint32_t          count = 0;
				CellFieldRampStop stops[16];
				uint32_t          revision = 0;
			};

			/// Bake a ramp into 256 RGBA8 texels (linear per channel, ends clamped). stops must be valid (count >= 1, sorted).
			void BakeRamp(const CellFieldRampStop* stops, uint32_t count, uint32_t outRgba8[256]);
			/// Bake a palette into 64 RGBA8 texels, padded with the fallback colour.
			void BakePalette(const uint32_t* rgba8, uint32_t count, uint32_t fallbackRgba8, uint32_t outRgba8[64]);

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
			static const char* const kErrInvalidPalette;
			static const char* const kErrInvalidRamp;
			static const char* const kErrColourMapTableFull;

			static const uint32_t kMaxPaletteColours = 64;
			static const uint32_t kMaxRampStops = 16;
			static const uint32_t kMaxPalettes = 128;
			static const uint32_t kMaxRamps = 128;
			static const uint32_t kRampTexels = 256;

			CellFieldRegistry();
			~CellFieldRegistry();
			CellFieldRegistry(const CellFieldRegistry&) = delete;
			CellFieldRegistry& operator=(const CellFieldRegistry&) = delete;

			CellFieldResult CreateLayer(const CellGridDesc& desc, CellFieldValueKind kind, Dia::Core::CellFieldLayerHandle& out);
			CellFieldResult Update(Dia::Core::CellFieldLayerHandle handle, const float* values, uint32_t count);
			void            Destroy(Dia::Core::CellFieldLayerHandle handle);

			/// True if the handle refers to a live layer. Increments staleHandles when it does not.
			bool ResolveHandle(Dia::Core::CellFieldLayerHandle handle);

			/// Register (or replace, bumping revision) a palette keyed by id.Value(). 1..64 colours.
			CellFieldResult RegisterPalette(Dia::Core::StringCRC id, const uint32_t* rgba8, uint32_t count, uint32_t fallbackRgba8);
			/// Register (or replace, bumping revision) a ramp keyed by id.Value(). 1..16 stops, positions non-decreasing in [0,1].
			CellFieldResult RegisterRamp(Dia::Core::StringCRC id, const CellFieldRampStop* stops, uint32_t count);

			/// Thread-safe copy lookups by StringCRC value (as carried in CellFieldDrawRequest::colourMap).
			/// False if unknown; not counted in stats.
			bool GetPalette(uint32_t id, CellFieldPaletteData& out) const;
			bool GetRamp(uint32_t id, CellFieldRampData& out) const;

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
