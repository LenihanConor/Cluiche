#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <functional>
#include <string.h>
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

	CellDetailResult::CellDetailResult() : mRows(new CellDetailRow[kMaxRows]) {}
	CellDetailResult::~CellDetailResult() { delete[] mRows; }
	void CellDetailResult::Clear() { mCount = 0; mTruncated = false; }
	CellDetailRow* CellDetailResult::Add()
	{
		if (mCount >= kMaxRows) { mTruncated = true; return nullptr; }
		mRows[mCount] = CellDetailRow();
		return &mRows[mCount++];
	}

	CellDetailBuilder::CellDetailBuilder(CellDetailResult& out, const Dia::ValueSchema::ValueSchemaService* schema, uint32_t maxRows)
		: mOut(out), mSchema(schema), mMaxRows(maxRows) {}

	CellDetailRow* CellDetailBuilder::Next()
	{
		if (mAdded >= mMaxRows) { ++mDropped; return nullptr; }
		CellDetailRow* row = mOut.Add();
		if (row == nullptr) { ++mDropped; return nullptr; }	// whole result full
		++mAdded;
		return row;
	}

	void CellDetailBuilder::Section(const char* title)
	{
		CellDetailRow* row = Next();
		if (row == nullptr) { return; }
		row->kind = CellDetailRowKind::Section;
		Dia::ValueSchema::SetText(row->label, title);
	}

	void CellDetailBuilder::RowValue(StringCRC schemaKey, float value)
	{
		CellDetailRow* row = Next();
		if (row == nullptr) { return; }
		row->kind = CellDetailRowKind::Value;
		row->schemaKey = schemaKey;
		row->value = value;
		const Dia::ValueSchema::ValueInfo* info = mSchema ? mSchema->Find(schemaKey) : nullptr;
		if (info == nullptr) { return; }
		row->resolved = true;
		Dia::ValueSchema::SetText(row->label, info->mLabel.AsCStr());
		Dia::ValueSchema::SetText(row->unit, info->mUnit.AsCStr());
		const char* tip = info->mHint.AsCStr()[0] != '\0' ? info->mHint.AsCStr() : info->mDescription.AsCStr();
		Dia::ValueSchema::SetText(row->tooltip, tip);
	}

	void CellDetailBuilder::Row(const char* label, const char* text)
	{
		CellDetailRow* row = Next();
		if (row == nullptr) { return; }
		row->kind = CellDetailRowKind::Text;
		Dia::ValueSchema::SetText(row->label, label);
		Dia::ValueSchema::SetText(row->text, text);
	}

	CellDetailRegistry::CellDetailRegistry(CurrentPuIdFn currentPuId)
		: mCurrentPuId(currentPuId ? currentPuId : &DefaultPuId)
		, mOwnerPuId(mCurrentPuId())
	{
	}

	CellDetailRegisterResult CellDetailRegistry::Register(const Dia::Graphics::CellGridDesc* grid, const char* providerName, CellDetailProviderFn fn)
	{
		if (grid == nullptr || providerName == nullptr || providerName[0] == '\0' || fn.empty())
		{
			DIA_LOG_ERROR("CEL", "Register detail provider rejected: null grid, empty name or empty callback");
			return CellDetailRegisterResult::Duplicate;
		}
		const uint64_t caller = mCurrentPuId();
		if (caller != mOwnerPuId)
		{
			DIA_LOG_ERROR("CEL", "Register detail provider '%s' rejected: called from PU %llu, registry owned by PU %llu",
				providerName, static_cast<unsigned long long>(caller), static_cast<unsigned long long>(mOwnerPuId));
			return CellDetailRegisterResult::WrongProcessingUnit;
		}
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mProviders[i].grid == grid && strcmp(mProviders[i].name.AsCStr(), providerName) == 0)
			{
				DIA_LOG_ERROR("CEL", "Register detail provider '%s' rejected: duplicate name on this grid", providerName);
				return CellDetailRegisterResult::Duplicate;
			}
		}
		if (mCount >= kMaxProviders)
		{
			DIA_LOG_ERROR("CEL", "Register detail provider '%s' rejected: capacity %u reached", providerName, kMaxProviders);
			return CellDetailRegisterResult::CapacityReached;
		}
		Provider& p = mProviders[mCount++];
		p.grid = grid;
		Dia::ValueSchema::SetText(p.name, providerName);
		p.fn = fn;
		p.warned = false;
		return CellDetailRegisterResult::Ok;
	}

	bool CellDetailRegistry::IsOwnerPu(const char* what) const
	{
		const uint64_t caller = mCurrentPuId();
		if (caller == mOwnerPuId) { return true; }
		DIA_LOG_ERROR("CEL", "%s detail provider rejected: called from PU %llu, registry owned by PU %llu",
			what, static_cast<unsigned long long>(caller), static_cast<unsigned long long>(mOwnerPuId));
		return false;
	}

	bool CellDetailRegistry::Unregister(const Dia::Graphics::CellGridDesc* grid, const char* providerName)
	{
		if (!IsOwnerPu("Unregister")) { return false; }
		if (providerName == nullptr) { return false; }
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mProviders[i].grid == grid && strcmp(mProviders[i].name.AsCStr(), providerName) == 0)
			{
				for (uint32_t j = i + 1; j < mCount; ++j) { mProviders[j - 1] = mProviders[j]; }
				mProviders[--mCount] = Provider();
				return true;
			}
		}
		return false;
	}

	uint32_t CellDetailRegistry::UnregisterAll(const Dia::Graphics::CellGridDesc* grid)
	{
		if (!IsOwnerPu("UnregisterAll")) { return 0; }
		uint32_t removed = 0;
		uint32_t w = 0;
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mProviders[i].grid == grid) { ++removed; continue; }
			if (w != i) { mProviders[w] = mProviders[i]; }
			++w;
		}
		for (uint32_t i = w; i < mCount; ++i) { mProviders[i] = Provider(); }
		mCount = w;
		return removed;
	}

	uint32_t CellDetailRegistry::ProviderCountForGrid(const Dia::Graphics::CellGridDesc* grid) const
	{
		uint32_t n = 0;
		for (uint32_t i = 0; i < mCount; ++i) { if (mProviders[i].grid == grid) { ++n; } }
		return n;
	}

	uint32_t CellDetailRegistry::Build(const Dia::Graphics::CellGridDesc* grid, uint32_t cellIndex,
		const Dia::ValueSchema::ValueSchemaService* schema, CellDetailResult& out)
	{
		out.Clear();
		uint32_t called = 0;
		// Providers may (un)register during their callback, so iterate over a snapshot.
		Provider snapshot[kMaxProviders];
		uint32_t snapshotCount = 0;
		for (uint32_t i = 0; i < mCount; ++i)
		{
			if (mProviders[i].grid == grid) { snapshot[snapshotCount++] = mProviders[i]; }
		}
		for (uint32_t i = 0; i < snapshotCount; ++i)
		{
			Provider& p = snapshot[i];
			if (CellDetailRow* header = out.Add())
			{
				header->kind = CellDetailRowKind::Section;
				Dia::ValueSchema::SetText(header->label, p.name.AsCStr());
			}
			CellDetailBuilder builder(out, schema);
			{
				DIA_PROFILE_SCOPE("celllayers.detail.provider", ::Dia::Observation::Profile::Category::kDiaGraphics);
				p.fn(cellIndex, builder);
			}
			++called;
			if (builder.Truncated() && !p.warned)
			{
				p.warned = true;
				for (uint32_t j = 0; j < mCount; ++j)
				{
					if (mProviders[j].grid == grid && strcmp(mProviders[j].name.AsCStr(), p.name.AsCStr()) == 0) { mProviders[j].warned = true; }
				}
				DIA_LOG_WARNING("CEL", "Detail provider '%s' added more than %u rows; %u dropped (warning once)",
					p.name.AsCStr(), CellDetailBuilder::kMaxRowsPerProvider, builder.Dropped());
			}
		}
		return called;
	}

	bool ShouldRunProviders(const ICellSelectionView* selection, bool cardOpen, uint32_t& cellIndex)
	{
		if (selection == nullptr || !cardOpen || selection->Count() != 1) { return false; }
		selection->ForEach([](uint32_t i, void* user) { *static_cast<uint32_t*>(user) = i; }, &cellIndex);
		return true;
	}
}
