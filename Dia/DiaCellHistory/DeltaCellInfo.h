#pragma once

// DeltaCellInfo (Feature 4, task 4.8): the per-cell side values behind the delta hover tooltip and the selected-cell detail rows.
//
// DeltaCellReader::Get answers "what were A and B and the delta for ONE cell of the delta layer". It reads each side of the whole
// layer once through SideReader and caches both reads keyed on (view epoch, layer id, layer version(), per-cell window start), so
// reading every cell of an unchanged view costs no further reads. A side with no data reports hasData false (never live values);
// delta is NaN unless both sides have data and the layer is a float layer (a category layer has no delta). Two scratch buffers of
// CellCount floats are allocated on first use. Same-PU contract as the recorder; no singletons.
//
// DeltaCellDetails registers a CellDetailProvider ("History delta") on the delta layer's grid while the view is in delta mode and
// removes it when it leaves, so the Selection Inspector's single-cell card shows "A (<side>)", "B (<side>)" and "Delta" rows with no
// stray empty section outside delta mode. Call Sync(registry) once per frame (CellHistoryDomain::Update does).

#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/SideReader.h>
#include <DiaCellLayers/CellDetailRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia::CellLayers { struct CellLayerDesc; class CellLayerRegistry; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class SnapshotCellCache;

	struct DeltaCellInfo
	{
		float	a = 0.0f;
		float	b = 0.0f;
		float	delta = 0.0f;		// b - a; NaN unless both sides have data (and the layer is a float layer)
		bool	aHasData = false;
		bool	bHasData = false;
	};

	class DeltaCellReader
	{
	public:
		explicit DeltaCellReader(const CellHistoryRecorder& recorder);
		~DeltaCellReader();
		DeltaCellReader(const DeltaCellReader&) = delete;
		DeltaCellReader& operator=(const DeltaCellReader&) = delete;

		// Optional, borrowed: where snapshot sides are read from (see SideReader). nullptr clears.
		void		SetSnapshotCache(SnapshotCellCache* cache) { mReader.SetSnapshotCache(cache); }

		// False when cell >= CellCount (out untouched).
		bool		Get(const Dia::CellLayers::CellLayerDesc& layer, const DeltaSpec& delta, uint64_t viewEpoch, uint32_t cell, DeltaCellInfo& out);
		uint32_t	ReadCount() const { return mReads; }	// SideReader reads performed (two per refresh)

	private:
		const CellHistoryRecorder&	mRecorder;
		SideReader					mReader;
		float*						mBufA = nullptr;
		float*						mBufB = nullptr;
		uint32_t					mBufCount = 0;
		bool						mOkA = false;
		bool						mOkB = false;
		bool						mValid = false;
		uint64_t					mKeyEpoch = 0;
		uint64_t					mKeyVersion = 0;
		uint32_t					mKeyFirstRetained = 0;
		Dia::Core::StringCRC		mKeyLayer;
		uint32_t					mReads = 0;
	};

	class DeltaCellDetails
	{
	public:
		DeltaCellDetails(const CellHistoryRecorder& recorder, const HistoryViewState& view);
		void		SetSnapshotCache(SnapshotCellCache* cache) { mCells.SetSnapshotCache(cache); }
		~DeltaCellDetails();
		DeltaCellDetails(const DeltaCellDetails&) = delete;
		DeltaCellDetails& operator=(const DeltaCellDetails&) = delete;

		// Registers/unregisters the provider to match the view (in delta mode: on the delta layer's grid). registry may be null.
		void		Sync(Dia::CellLayers::CellLayerRegistry* registry);
		bool		IsRegistered() const { return mGrid != nullptr; }

	private:
		void		Provide(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& builder);
		void		Unregister();

		const CellHistoryRecorder&	mRecorder;
		const HistoryViewState&		mView;
		DeltaCellReader				mCells;
		Dia::CellLayers::CellLayerRegistry*			mRegistry = nullptr;
		const Dia::Graphics::CellGridDesc*			mGrid = nullptr;
	};
}
