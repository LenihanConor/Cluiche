////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldRegistry.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/CellField/CellFieldRegistry.h"

#include <DiaObservation/Log/DiaLog.h>

#include <atomic>
#include <mutex>
#include <string.h>
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
			};
		}

		struct CellFieldRegistry::Impl
		{
			mutable std::mutex   mMutex;
			Slot                 mSlots[kMaxLayers];
			std::vector<Command> mQueue;
			uint64_t             mQueuedBytes = 0;
			bool                 mDropLogged = false;
			CellFieldStats       mStats;

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
					DIA_LOG_WARNING("CFL", "Command queue byte cap reached; dropping commands");
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
			std::lock_guard<std::mutex> lock(m.mMutex);
			if (!m.IsLive(handle))
			{
				++m.mStats.staleHandles;
				r.error = kErrStaleHandle;
				return r;
			}
			if (values == nullptr || count != m.mSlots[handle.index].cellCount)
			{
				++m.mStats.rejectedRequests;
				DIA_LOG_WARNING("CFL", "Update rejected: value count mismatch (%u)", (unsigned)count);
				r.error = kErrCountMismatch;
				return r;
			}
			const uint64_t bytes = sizeof(float) * (uint64_t)count;
			if (m.mQueuedBytes + bytes > kMaxQueueBytes)
			{
				++m.mStats.droppedCommands;
				m.LogDropOnce();
				r.error = kErrQueueFull;
				return r;
			}
			Command cmd;
			cmd.type = CmdType::Update;
			cmd.handle = handle;
			cmd.values.assign(values, values + count);
			cmd.bytes = bytes;
			m.mQueuedBytes += bytes;
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
