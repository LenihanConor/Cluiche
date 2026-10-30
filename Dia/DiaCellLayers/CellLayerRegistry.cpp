#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <functional>
#include <thread>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	namespace
	{
		uint64_t DefaultPuId()
		{
			return static_cast<uint64_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
		}
	}

	CellLayerRegistry::CellLayerRegistry(const Dia::ValueSchema::ValueSchemaService* schema, CurrentPuIdFn currentPuId)
		: mSchema(schema)
		, mCurrentPuId(currentPuId ? currentPuId : &DefaultPuId)
		, mOwnerPuId(mCurrentPuId())
		, mDetails(mCurrentPuId)
	{
	}

	CellLayerRegistry::~CellLayerRegistry() {}

	bool CellLayerRegistry::IsOwnerPu(const char* what, StringCRC id) const
	{
		const uint64_t caller = mCurrentPuId();
		if (caller == mOwnerPuId) { return true; }
		DIA_LOG_ERROR("CEL", "Register %s '%s' rejected: called from PU %llu, registry owned by PU %llu",
			what, id.AsChar(), static_cast<unsigned long long>(caller), static_cast<unsigned long long>(mOwnerPuId));
		return false;
	}

	bool CellLayerRegistry::SetViewValueSource(IViewValueSource* source)
	{
		if (source == nullptr)
		{
			DIA_LOG_ERROR("CEL", "SetViewValueSource rejected: null source (use ClearViewValueSource)");
			return false;
		}
		if (mViewSource != nullptr)
		{
			DIA_LOG_ERROR("CEL", "SetViewValueSource rejected: a view value source is already installed");
			return false;
		}
		mViewSource = source;
		return true;
	}

	CellRegisterResult CellLayerRegistry::Register(const CellLayerDesc& desc)
	{
		if (!IsOwnerPu("layer", desc.id)) { Reject(); return CellRegisterResult::WrongProcessingUnit; }
		if (Find(desc.id) != nullptr)
		{
			DIA_LOG_ERROR("CEL", "Register layer '%s' rejected: duplicate id", desc.id.AsChar());
			Reject();
			return CellRegisterResult::Duplicate;
		}
		if (mLayerCount >= kMaxLayers)
		{
			DIA_LOG_ERROR("CEL", "Register layer '%s' rejected: capacity %u reached", desc.id.AsChar(), kMaxLayers);
			Reject();
			return CellRegisterResult::CapacityReached;
		}
		mLayers[mLayerCount++] = desc;
		++mLayersVersion;
		return CellRegisterResult::Ok;
	}

	bool CellLayerRegistry::Unregister(StringCRC id)
	{
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (mLayers[i].id == id)
			{
				for (uint32_t j = i + 1; j < mLayerCount; ++j) { mLayers[j - 1] = mLayers[j]; }
				mLayers[--mLayerCount] = CellLayerDesc();
				++mLayersVersion;
				return true;
			}
		}
		return false;
	}

	const CellLayerDesc* CellLayerRegistry::Find(StringCRC id) const
	{
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (mLayers[i].id == id) { return &mLayers[i]; }
		}
		return nullptr;
	}

	const CellLayerDesc* CellLayerRegistry::LayerAt(uint32_t index) const
	{
		return index < mLayerCount ? &mLayers[index] : nullptr;
	}

	CellRegisterResult CellLayerRegistry::RegisterOverlay(const CellOverlayDesc& desc)
	{
		if (!IsOwnerPu("overlay", desc.id)) { Reject(); return CellRegisterResult::WrongProcessingUnit; }
		if (FindOverlay(desc.id) != nullptr)
		{
			DIA_LOG_ERROR("CEL", "Register overlay '%s' rejected: duplicate id", desc.id.AsChar());
			Reject();
			return CellRegisterResult::Duplicate;
		}
		if (mOverlayCount >= kMaxOverlays)
		{
			DIA_LOG_ERROR("CEL", "Register overlay '%s' rejected: capacity %u reached", desc.id.AsChar(), kMaxOverlays);
			Reject();
			return CellRegisterResult::CapacityReached;
		}
		mOverlays[mOverlayCount++] = desc;
		++mLayersVersion;
		return CellRegisterResult::Ok;
	}

	bool CellLayerRegistry::UnregisterOverlay(StringCRC id)
	{
		for (uint32_t i = 0; i < mOverlayCount; ++i)
		{
			if (mOverlays[i].id == id)
			{
				for (uint32_t j = i + 1; j < mOverlayCount; ++j) { mOverlays[j - 1] = mOverlays[j]; }
				mOverlays[--mOverlayCount] = CellOverlayDesc();
				++mLayersVersion;
				return true;
			}
		}
		return false;
	}

	const CellOverlayDesc* CellLayerRegistry::FindOverlay(StringCRC id) const
	{
		for (uint32_t i = 0; i < mOverlayCount; ++i)
		{
			if (mOverlays[i].id == id) { return &mOverlays[i]; }
		}
		return nullptr;
	}

	const CellOverlayDesc* CellLayerRegistry::OverlayAt(uint32_t index) const
	{
		return index < mOverlayCount ? &mOverlays[index] : nullptr;
	}

	bool CellLayerRegistry::IsFirstOfGrid(uint32_t layerIndex) const
	{
		for (uint32_t j = 0; j < layerIndex; ++j)
		{
			if (mLayers[j].grid == mLayers[layerIndex].grid) { return false; }
		}
		return true;
	}

	uint32_t CellLayerRegistry::GridCount() const
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < mLayerCount; ++i) { if (IsFirstOfGrid(i)) { ++count; } }
		return count;
	}

	const Dia::Graphics::CellGridDesc* CellLayerRegistry::GridAt(uint32_t index) const
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (!IsFirstOfGrid(i)) { continue; }
			if (count == index) { return mLayers[i].grid; }
			++count;
		}
		return nullptr;
	}

	uint32_t CellLayerRegistry::LayerCountForGrid(const Dia::Graphics::CellGridDesc* grid) const
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < mLayerCount; ++i) { if (mLayers[i].grid == grid) { ++count; } }
		return count;
	}

	const CellLayerDesc* CellLayerRegistry::LayerOfGrid(const Dia::Graphics::CellGridDesc* grid, uint32_t index) const
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (mLayers[i].grid != grid) { continue; }
			if (count == index) { return &mLayers[i]; }
			++count;
		}
		return nullptr;
	}

	bool CellLayerRegistry::HasDefaultBase(const CellLayerDesc& desc) const
	{
		if (mSchema == nullptr) { return false; }
		const Dia::ValueSchema::ValueInfo* info = mSchema->Find(desc.schemaKey);
		if (info == nullptr) { return false; }
		const Json::Value& flag = info->mUi["default_base"];
		return flag.isBool() && flag.asBool();
	}

	const CellLayerDesc* CellLayerRegistry::DefaultBase(const Dia::Graphics::CellGridDesc* grid) const
	{
		const CellLayerDesc* first = nullptr;
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (mLayers[i].grid != grid) { continue; }
			if (first == nullptr) { first = &mLayers[i]; }
			if (HasDefaultBase(mLayers[i])) { return &mLayers[i]; }
		}
		return first;
	}
	CellLayerRegistry::SelectionEntry* CellLayerRegistry::FindSelection(const Dia::Graphics::CellGridDesc* grid) const
	{
		for (uint32_t i = 0; i < kMaxSelectionGrids; ++i)
		{
			if (mSelections[i].grid == grid) { return &mSelections[i]; }
		}
		return nullptr;
	}

	void CellLayerRegistry::ResetEntry(SelectionEntry& e)
	{
		const uint32_t before = e.selection.Count();
		e.selection.Reset(e.grid);	// no-op when width/height/topology are unchanged
		if (before > 0 && e.selection.Count() == 0)
		{
			DIA_LOG_INFO("CEL", "Selection cleared: grid size or id changed (%u cells dropped)", before);
		}
	}

	CellSelection* CellLayerRegistry::SelectionFor(const Dia::Graphics::CellGridDesc* grid)
	{
		if (grid == nullptr) { return nullptr; }
		SelectionEntry* e = FindSelection(grid);
		if (e == nullptr)
		{
			e = FindSelection(nullptr);	// a free slot
			if (e == nullptr)
			{
				if (!mSelectionCapacityLogged)
				{
					DIA_LOG_WARNING("CEL", "Selection store full (%u grids); no selection for further grids", kMaxSelectionGrids);
					mSelectionCapacityLogged = true;
				}
				return nullptr;
			}
			e->grid = grid;
		}
		ResetEntry(*e);
		return &e->selection;
	}

	const ICellSelectionView* CellLayerRegistry::SelectionView(const Dia::Graphics::CellGridDesc* grid) const
	{
		if (grid == nullptr || LayerCountForGrid(grid) == 0) { return nullptr; }
		SelectionEntry* e = FindSelection(grid);
		if (e == nullptr) { return nullptr; }
		ResetEntry(*e);	// never hand out a selection sized for a stale grid
		return &e->selection;
	}
}
