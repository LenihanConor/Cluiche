////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRegistry.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/CellField/CellFieldRegistry.h"

#include <DiaObservation/Log/DiaLog.h>

#include <atomic>
#include <mutex>
#include <math.h>
#include <string.h>
#include <utility>
#include <vector>

namespace Dia
{
	namespace Graphics
	{
		const char* const CellFieldRegistry::kErrMaxLayers = "Max 32 layers";
		// "Grid exceeds 128" + U+00D7 (UTF-8) + "128", escaped to stay source-charset independent
		const char* const CellFieldRegistry::kErrGridTooLarge = "Grid exceeds 128" "\xC3\x97" "128";
		const char* const CellFieldRegistry::kErrInvalidGrid = "Invalid grid description";
		const char* const CellFieldRegistry::kErrCountMismatch = "Value count mismatch";
		const char* const CellFieldRegistry::kErrStaleHandle = "Stale handle";
		const char* const CellFieldRegistry::kErrQueueFull = "Command queue full";
		const char* const CellFieldRegistry::kErrInvalidPalette = "Invalid palette";
		const char* const CellFieldRegistry::kErrInvalidRamp = "Invalid ramp";
		const char* const CellFieldRegistry::kErrColourMapTableFull = "Colour map table full";

		namespace
		{
			enum class CmdType : uint8_t { Create, Update, Destroy };

			struct Command
			{
				CmdType                         type = CmdType::Create;
				Dia::Core::CellFieldLayerHandle handle;
				CellFieldValueKind              kind = CellFieldValueKind::Category;
				CellGridDesc                    desc;	// pointers re-targeted at the vectors below
				std::vector<float>              centres;
				std::vector<int32_t>            neighbours;
				std::vector<float>              values;
				uint64_t                        bytes = 0;
			};

			struct Slot
			{
				std::atomic<uint16_t> generation{ 0 };
				bool                  allocated = false;	// held until the Destroy is drained
				bool                  live = false;			// handle resolves
				uint32_t              cellCount = 0;
				int32_t               pendingUpdate = -1;	// index in mQueue of this layer's pending Update, or -1
			};
		}

		struct CellFieldRegistry::Impl
		{
			mutable std::mutex   mMutex;
			Slot                 mSlots[kMaxLayers];
			std::vector<Command> mQueue;
			uint64_t             mQueuedBytes = 0;
			bool                 mDropLogged = false;
			bool                 mCoalesceLogged = false;
			CellFieldStats       mStats;
			std::vector<std::pair<uint32_t, CellFieldPaletteData>> mPalettes;
			std::vector<std::pair<uint32_t, CellFieldRampData>>    mRamps;

			bool IsLive(Dia::Core::CellFieldLayerHandle h) const
			{
				return h.index < kMaxLayers && mSlots[h.index].live
					&& mSlots[h.index].generation.load() == h.generation;
			}

			void LogDropOnce()
			{
				if (!mDropLogged)
				{
					mDropLogged = true;
					++mStats.capWarningsLogged;
					DIA_LOG_WARNING("CFL", "Command queue byte cap reached; dropping commands");
				}
			}

			void LogCoalesceOnce()
			{
				if (!mCoalesceLogged)
				{
					mCoalesceLogged = true;
					DIA_LOG_INFO("CFL", "Upload queue coalescing Updates (further merges this cycle not logged)");
				}
			}
		};

		CellFieldRegistry::CellFieldRegistry() : mImpl(new Impl()) {}
		CellFieldRegistry::~CellFieldRegistry() { delete mImpl; }

		//------------------------------------------------------------------------------
		CellFieldResult CellFieldRegistry::CreateLayer(const CellGridDesc& desc, CellFieldValueKind kind, Dia::Core::CellFieldLayerHandle& out)
		{
			out = Dia::Core::CellFieldLayerHandle();
			CellFieldResult r;
			Impl& m = *mImpl;

			if (desc.width > kMaxGridDim || desc.height > kMaxGridDim)
			{
				std::lock_guard<std::mutex> lock(m.mMutex);
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "CreateLayer rejected: grid %ux%u exceeds limit", (unsigned)desc.width, (unsigned)desc.height);
				r.error = kErrGridTooLarge;
				return r;
			}
			if (desc.width == 0 || desc.height == 0 || desc.centresXY == nullptr || desc.neighbours == nullptr)
			{
				std::lock_guard<std::mutex> lock(m.mMutex);
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "CreateLayer rejected: invalid grid description");
				r.error = kErrInvalidGrid;
				return r;
			}

			const uint32_t cells = (uint32_t)desc.width * desc.height;
			Command cmd;
			cmd.type = CmdType::Create;
			cmd.kind = kind;
			cmd.desc = desc;
			cmd.centres.assign(desc.centresXY, desc.centresXY + 2 * cells);
			cmd.neighbours.assign(desc.neighbours, desc.neighbours + 6 * cells);
			cmd.desc.centresXY = cmd.centres.data();
			cmd.desc.neighbours = cmd.neighbours.data();
			cmd.bytes = sizeof(float) * 2ull * cells + sizeof(int32_t) * 6ull * cells;

			std::lock_guard<std::mutex> lock(m.mMutex);
			if (m.mQueuedBytes + cmd.bytes > kMaxQueueBytes)
			{
				++m.mStats.droppedCommands;
				m.LogDropOnce();
				r.error = kErrQueueFull;
				return r;
			}
			uint32_t freeSlot = kMaxLayers;
			for (uint32_t i = 0; i < kMaxLayers; ++i)
			{
				if (!m.mSlots[i].allocated) { freeSlot = i; break; }
			}
			if (freeSlot == kMaxLayers)
			{
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "CreateLayer rejected: max %u layers", kMaxLayers);
				r.error = kErrMaxLayers;
				return r;
			}

			Slot& s = m.mSlots[freeSlot];
			s.allocated = true;
			s.live = true;
			s.cellCount = cells;
			out.index = (uint16_t)freeSlot;
			out.generation = s.generation.load();
			cmd.handle = out;
			m.mQueuedBytes += cmd.bytes;
			m.mQueue.push_back(std::move(cmd));
			++m.mStats.creates;
			r.ok = true;
			return r;
		}

		//------------------------------------------------------------------------------
		CellFieldResult CellFieldRegistry::Update(Dia::Core::CellFieldLayerHandle handle, const float* values, uint32_t count)
		{
			CellFieldResult r;
			Impl& m = *mImpl;

			// Copy-on-submit happens BEFORE the lock so the mutex is held only for bookkeeping
			// (a 64 KB memcpy/allocation never runs under it). Counts above the largest grid
			// can never match a layer, so they skip the copy and are rejected under the lock.
			Command cmd;	// declared before the lock: its (possibly swapped-out) buffer is freed after unlock
			cmd.type = CmdType::Update;
			cmd.handle = handle;
			const uint32_t maxCells = (uint32_t)kMaxGridDim * kMaxGridDim;
			if (values != nullptr && count <= maxCells)
			{
				cmd.values.assign(values, values + count);
			}
			const uint64_t bytes = sizeof(float) * (uint64_t)count;
			cmd.bytes = bytes;

			std::lock_guard<std::mutex> lock(m.mMutex);
			if (!m.IsLive(handle))
			{
				++m.mStats.staleHandles;
				r.error = kErrStaleHandle;
				return r;
			}
			Slot& slot = m.mSlots[handle.index];
			if (values == nullptr || count != slot.cellCount)
			{
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "Update rejected: value count mismatch (%u)", (unsigned)count);
				r.error = kErrCountMismatch;
				return r;
			}
			if (slot.pendingUpdate >= 0)
			{
				// Coalesce: overwrite the pending Update in place. It keeps the FIRST pending
				// Update's queue position, which is always after the layer's Create and before
				// its Destroy (a Destroy makes the handle stale, so no Update can follow it).
				// Same cellCount => same byte cost, so this can never trip the cap.
				m.mQueue[(size_t)slot.pendingUpdate].values.swap(cmd.values);
				++m.mStats.updates;
				++m.mStats.coalescedCommands;
				m.LogCoalesceOnce();
				r.ok = true;
				return r;
			}
			if (m.mQueuedBytes + bytes > kMaxQueueBytes)
			{
				++m.mStats.droppedCommands;
				m.LogDropOnce();
				r.error = kErrQueueFull;
				return r;
			}
			m.mQueuedBytes += bytes;
			slot.pendingUpdate = (int32_t)m.mQueue.size();
			m.mQueue.push_back(std::move(cmd));
			++m.mStats.updates;
			r.ok = true;
			return r;
		}

		//------------------------------------------------------------------------------
		void CellFieldRegistry::Destroy(Dia::Core::CellFieldLayerHandle handle)
		{
			Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			if (!m.IsLive(handle))
			{
				++m.mStats.staleHandles;
				return;
			}
			m.mSlots[handle.index].pendingUpdate = -1;
			m.mSlots[handle.index].live = false;	// stops resolving now; slot freed at drain
			Command cmd;
			cmd.type = CmdType::Destroy;
			cmd.handle = handle;
			m.mQueue.push_back(std::move(cmd));		// no byte cost: must never be dropped
			++m.mStats.destroys;
		}

		//------------------------------------------------------------------------------
		bool CellFieldRegistry::ResolveHandle(Dia::Core::CellFieldLayerHandle handle)
		{
			Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			if (m.IsLive(handle)) { return true; }
			++m.mStats.staleHandles;
			return false;
		}

		//------------------------------------------------------------------------------
		void CellFieldRegistry::Drain(ICellFieldCommandSink& sink)
		{
			Impl& m = *mImpl;
			std::vector<Command> local;
			{
				std::lock_guard<std::mutex> lock(m.mMutex);
				local.swap(m.mQueue);
				m.mQueuedBytes = 0;
				m.mDropLogged = false;
				m.mCoalesceLogged = false;
				for (uint32_t i = 0; i < kMaxLayers; ++i) { m.mSlots[i].pendingUpdate = -1; }
			}
			uint64_t uploads = 0, uploadedBytes = 0, builds = 0;
			for (Command& c : local)
			{
				switch (c.type)
				{
				case CmdType::Create:
					// Re-target here: Command may have been copied (not moved) while queued
					c.desc.centresXY = c.centres.data();
					c.desc.neighbours = c.neighbours.data();
					sink.OnCreate(c.handle, c.desc, c.kind);
					++builds;
					break;
				case CmdType::Update:
					sink.OnUpdate(c.handle, c.values.data(), (uint32_t)c.values.size());
					++uploads;
					uploadedBytes += c.values.size() * sizeof(float);
					break;
				case CmdType::Destroy:
				{
					sink.OnDestroy(c.handle);
					std::lock_guard<std::mutex> lock(m.mMutex);
					Slot& s = m.mSlots[c.handle.index];
					s.generation.store((uint16_t)(s.generation.load() + 1));
					s.allocated = false;
					s.live = false;
					s.cellCount = 0;
					break;
				}
				}
			}
			std::lock_guard<std::mutex> lock(m.mMutex);
			m.mStats.uploads += uploads;
			m.mStats.uploadedBytes += uploadedBytes;
			m.mStats.geometryBuilds += builds;
		}

		//------------------------------------------------------------------------------
		namespace
		{
			uint32_t LerpPacked(uint32_t a, uint32_t b, float t)
			{
				uint32_t out = 0;
				for (int shift = 0; shift <= 24; shift += 8)
				{
					const float ca = (float)((a >> shift) & 0xFFu);
					const float cb = (float)((b >> shift) & 0xFFu);
					int v = (int)floorf(ca + (cb - ca) * t + 0.5f);
					v = v < 0 ? 0 : (v > 255 ? 255 : v);
					out |= (uint32_t)v << shift;
				}
				return out;
			}
		}

		void BakeRamp(const CellFieldRampStop* stops, uint32_t count, uint32_t outRgba8[256])
		{
			for (uint32_t i = 0; i < CellFieldRegistry::kRampTexels; ++i)
			{
				const float u = (float)i / (float)(CellFieldRegistry::kRampTexels - 1);
				if (u <= stops[0].position) { outRgba8[i] = stops[0].rgba8; continue; }
				if (u >= stops[count - 1].position) { outRgba8[i] = stops[count - 1].rgba8; continue; }
				uint32_t k = 1;
				while (k < count - 1 && u > stops[k].position) { ++k; }
				const float span = stops[k].position - stops[k - 1].position;
				const float t = span > 0.0f ? (u - stops[k - 1].position) / span : 1.0f;
				outRgba8[i] = LerpPacked(stops[k - 1].rgba8, stops[k].rgba8, t);
			}
		}

		void BakePalette(const uint32_t* rgba8, uint32_t count, uint32_t fallbackRgba8, uint32_t outRgba8[64])
		{
			for (uint32_t i = 0; i < CellFieldRegistry::kMaxPaletteColours; ++i)
			{
				outRgba8[i] = i < count ? rgba8[i] : fallbackRgba8;
			}
		}

		//------------------------------------------------------------------------------
		CellFieldResult CellFieldRegistry::RegisterPalette(Dia::Core::StringCRC id, const uint32_t* rgba8, uint32_t count, uint32_t fallbackRgba8)
		{
			CellFieldResult r;
			Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			if (rgba8 == nullptr || count == 0 || count > kMaxPaletteColours)
			{
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "RegisterPalette rejected: invalid colour count %u", (unsigned)count);
				r.error = kErrInvalidPalette;
				return r;
			}
			const uint32_t key = id.Value();
			std::pair<uint32_t, CellFieldPaletteData>* entry = nullptr;
			for (auto& e : m.mPalettes) { if (e.first == key) { entry = &e; break; } }
			if (entry == nullptr)
			{
				if (m.mPalettes.size() >= kMaxPalettes)
				{
					++m.mStats.rejectedRequests;
					DIA_LOG_WARNING("CFL", "RegisterPalette rejected: table full");
					r.error = kErrColourMapTableFull;
					return r;
				}
				m.mPalettes.emplace_back(key, CellFieldPaletteData());
				entry = &m.mPalettes.back();
			}
			CellFieldPaletteData& d = entry->second;
			d.count = count;
			memcpy(d.colours, rgba8, sizeof(uint32_t) * count);
			d.fallback = fallbackRgba8;
			++d.revision;
			r.ok = true;
			return r;
		}

		//------------------------------------------------------------------------------
		CellFieldResult CellFieldRegistry::RegisterRamp(Dia::Core::StringCRC id, const CellFieldRampStop* stops, uint32_t count)
		{
			CellFieldResult r;
			Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			bool valid = stops != nullptr && count != 0 && count <= kMaxRampStops;
			for (uint32_t i = 0; valid && i < count; ++i)
			{
				const float p = stops[i].position;
				if (!(p >= 0.0f && p <= 1.0f) || (i > 0 && p < stops[i - 1].position)) { valid = false; }
			}
			if (!valid)
			{
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "RegisterRamp rejected: invalid stops (count %u)", (unsigned)count);
				r.error = kErrInvalidRamp;
				return r;
			}
			const uint32_t key = id.Value();
			std::pair<uint32_t, CellFieldRampData>* entry = nullptr;
			for (auto& e : m.mRamps) { if (e.first == key) { entry = &e; break; } }
			if (entry == nullptr)
			{
				if (m.mRamps.size() >= kMaxRamps)
				{
					++m.mStats.rejectedRequests;
					DIA_LOG_WARNING("CFL", "RegisterRamp rejected: table full");
					r.error = kErrColourMapTableFull;
					return r;
				}
				m.mRamps.emplace_back(key, CellFieldRampData());
				entry = &m.mRamps.back();
			}
			CellFieldRampData& d = entry->second;
			d.count = count;
			for (uint32_t i = 0; i < count; ++i) { d.stops[i] = stops[i]; }
			++d.revision;
			r.ok = true;
			return r;
		}

		//------------------------------------------------------------------------------
		bool CellFieldRegistry::GetPalette(uint32_t id, CellFieldPaletteData& out) const
		{
			const Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			for (const auto& e : m.mPalettes) { if (e.first == id) { out = e.second; return true; } }
			return false;
		}

		//------------------------------------------------------------------------------
		bool CellFieldRegistry::GetRamp(uint32_t id, CellFieldRampData& out) const
		{
			const Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			for (const auto& e : m.mRamps) { if (e.first == id) { out = e.second; return true; } }
			return false;
		}

		//------------------------------------------------------------------------------
		CellFieldStats CellFieldRegistry::GetStats() const
		{
			const Impl& m = *mImpl;
			std::lock_guard<std::mutex> lock(m.mMutex);
			CellFieldStats s = m.mStats;
			for (uint32_t i = 0; i < kMaxLayers; ++i)
			{
				if (m.mSlots[i].allocated) { ++s.layersAllocated; }
				if (m.mSlots[i].live) { ++s.layersLive; }
			}
			s.commandsQueued = (uint32_t)m.mQueue.size();
			s.queuedBytes = m.mQueuedBytes;
			return s;
		}
	}
}
