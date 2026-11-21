#pragma once

// CellDetailRegistry: systems register per-grid "detail providers" that fill the Selection Inspector's details section for a
// single selected cell. Owned by the CellLayerRegistry (CellLayerRegistry::Details(), CL-006: no statics); generic (CL-005);
// no STL in the API (PD-004). Providers are FastDelegates (bind with fastdelegate::MakeDelegate); the registered system must
// outlive its registration. Register / Unregister must be called from the owner PU (a call from any other PU is rejected with
// an Error on log channel CEL); Build runs on that same PU (the viewer's).
//
// Build(grid, cellIndex, schema, out) calls the grid's providers in registration order, each inside a DIA_PROFILE_SCOPE. Output
// layout in `out`: for each provider a Section row titled with its providerName, then the rows it added (its own Section()
// calls add further Section rows). A provider may add at most kMaxRowsPerProvider rows (Section rows included); extras are
// dropped and ONE Warning (channel CEL) is logged per provider for its lifetime, however many calls overflow. The result holds
// at most CellDetailResult::kMaxRows rows in total; rows past that are dropped (result.Truncated()).
// Capacity: kMaxProviders providers per registry (all grids); further registrations return CapacityReached.
// Gating ("exactly one cell selected and the card open") belongs to the caller: use ShouldRunProviders, and call Build only
// when it returns true.

#include <DiaCellLayers/ICellSelectionView.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String32.h>
#include <DiaGraphics/CellField/CellGridDesc.h>

#include <stdint.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	enum class CellDetailRowKind : uint8_t { Section, Value, Text };

	struct CellDetailRow
	{
		CellDetailRowKind					kind = CellDetailRowKind::Text;
		Dia::Core::StringCRC				schemaKey;		// Value rows only
		bool								resolved = false;	// Value rows: schemaKey found in the schema
		float								value = 0.0f;	// Value rows
		Dia::Core::Containers::String128	label;			// Section: title. Value: schema label (empty if unresolved). Text: label.
		Dia::Core::Containers::String32		unit;			// Value rows, from the schema
		Dia::Core::Containers::String128	tooltip;		// Value rows, from the schema (hint, else description), truncated
		Dia::Core::Containers::String128	text;			// Text rows: literal text
	};

	// Output of one Build. Heap-backed rows (the struct is large); not copyable.
	class CellDetailResult
	{
	public:
		static constexpr uint32_t kMaxRows = 256;

		CellDetailResult();
		~CellDetailResult();
		CellDetailResult(const CellDetailResult&) = delete;
		CellDetailResult& operator=(const CellDetailResult&) = delete;

		void				Clear();
		uint32_t			Count() const { return mCount; }
		const CellDetailRow&	At(uint32_t index) const { return mRows[index]; }	// index < Count()
		bool				Truncated() const { return mTruncated; }
		CellDetailRow*		Add();		// null (and Truncated set) when full

	private:
		CellDetailRow*		mRows;
		uint32_t			mCount = 0;
		bool				mTruncated = false;
	};

	// Collects the output of ONE provider call into a result. Rows beyond `maxRows` are dropped and counted.
	class CellDetailBuilder
	{
	public:
		static constexpr uint32_t kMaxRowsPerProvider = 64;

		CellDetailBuilder(CellDetailResult& out, const Dia::ValueSchema::ValueSchemaService* schema, uint32_t maxRows = kMaxRowsPerProvider);

		void		Section(const char* title);
		// Label, unit and tooltip come from the schema entry for schemaKey; if absent the row is kept with an empty label and
		// resolved == false.
		void		RowValue(Dia::Core::StringCRC schemaKey, float value);
		void		Row(const char* label, const char* text);

		uint32_t	Added() const { return mAdded; }
		uint32_t	Dropped() const { return mDropped; }
		bool		Truncated() const { return mDropped > 0; }

	private:
		CellDetailRow*	Next();

		CellDetailResult&	mOut;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		uint32_t			mMaxRows;
		uint32_t			mAdded = 0;
		uint32_t			mDropped = 0;
	};

	enum class CellDetailRegisterResult { Ok, Duplicate, CapacityReached, WrongProcessingUnit };

	typedef fastdelegate::FastDelegate2<uint32_t, CellDetailBuilder&> CellDetailProviderFn;

	class CellDetailRegistry
	{
	public:
		static constexpr uint32_t kMaxProviders = 16;
		typedef uint64_t (*CurrentPuIdFn)();	// same meaning as CellLayerRegistry::CurrentPuIdFn

		// The owner PU is the id returned by currentPuId at construction (null: hash of the calling thread id).
		explicit CellDetailRegistry(CurrentPuIdFn currentPuId = nullptr);
		CellDetailRegistry(const CellDetailRegistry&) = delete;
		CellDetailRegistry& operator=(const CellDetailRegistry&) = delete;

		// Duplicate = same (grid, providerName). A null grid / empty providerName / null fn is rejected (Error on CEL) as Duplicate.
		CellDetailRegisterResult	Register(const Dia::Graphics::CellGridDesc* grid, const char* providerName, CellDetailProviderFn fn);
		bool		Unregister(const Dia::Graphics::CellGridDesc* grid, const char* providerName);
		uint32_t	UnregisterAll(const Dia::Graphics::CellGridDesc* grid);	// returns the number removed
		uint32_t	ProviderCount() const { return mCount; }
		uint32_t	ProviderCountForGrid(const Dia::Graphics::CellGridDesc* grid) const;

		// Clears `out`, then runs the grid's providers (see header). Returns the number of providers called.
		uint32_t	Build(const Dia::Graphics::CellGridDesc* grid, uint32_t cellIndex,
						const Dia::ValueSchema::ValueSchemaService* schema, CellDetailResult& out);

	private:
		bool		IsOwnerPu(const char* what) const;

		struct Provider
		{
			const Dia::Graphics::CellGridDesc*	grid = nullptr;
			Dia::Core::Containers::String32		name;
			CellDetailProviderFn				fn;
			bool								warned = false;
		};
		CurrentPuIdFn	mCurrentPuId;
		uint64_t		mOwnerPuId;
		uint32_t		mCount = 0;
		Provider		mProviders[kMaxProviders];
	};

	// True only when `selection` is non-null with Count() == 1 and the card is open; outputs the single cell index.
	bool ShouldRunProviders(const ICellSelectionView* selection, bool cardOpen, uint32_t& cellIndex);
}
