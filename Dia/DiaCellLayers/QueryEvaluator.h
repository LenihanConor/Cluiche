#pragma once

// QueryEvaluator: evaluates a resolved cell query (QueryParser AST + QueryResolver bindings) to a bitset over the
// active grid's cells (cell index = r * width + q; the caller passes cellCount = width * height).
//
// Each Compare node reads its layer once through LayerReader (one pass over the values) and produces one bitset.
// Comparison precision: the threshold is narrowed to float and compared against the stored float, so `x <= 0.2`
// matches a stored 0.2f. Category layers use the SelectionTools rule: floorf(v + 0.5f) == (float)enumIndex.
// NaN semantics: a NaN cell is false for EVERY comparison operator including `!=` (and `==`). Because And/Or/Not are
// plain bitset operations, `not (x < 0.5)` is TRUE on a NaN cell (complement of false). That is the defined behaviour.
// Not results are masked to the valid cell count (tail bits of the last word are always zero).
//
// A layer with no data (hasData false) or a value count that differs from cellCount fails evaluation with a located
// QueryError ("no data for layer") at the name span of the offending Compare node.
//
// Storage: a pool of word buffers (stack-disciplined temps, depth <= kMaxDepth) is allocated on the first call and
// reused while the cell count is unchanged; no per-node allocation. Not thread-safe. No singletons (CL-006), generic (CL-005).

#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/QueryError.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/QueryResolver.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	class QueryEvaluator
	{
	public:
		QueryEvaluator() = default;
		~QueryEvaluator();
		QueryEvaluator(const QueryEvaluator&) = delete;
		QueryEvaluator& operator=(const QueryEvaluator&) = delete;

		// parser/resolver must hold a successfully parsed + resolved query. On failure Error() is set and the result is empty.
		bool				Evaluate(const QueryParser& parser, const QueryResolver& resolver, LayerReader& reader, uint32_t cellCount);

		uint32_t			CellCount() const { return mCellCount; }
		uint32_t			WordCount() const { return (mCellCount + 63u) / 64u; }
		const uint64_t*		Words() const { return mHasResult ? mPool : nullptr; }	// WordCount() words; tail bits zero
		bool				Test(uint32_t cell) const;
		uint32_t			Count() const;												// popcount of the result
		const QueryError&	Error() const { return mError; }

		// Replaces the selection's contents with the result (one BeginEdit/EndEdit: version bumps at most once, and
		// only if the set changed). False if there is no result or sel.CellCount() != CellCount().
		bool				ApplyTo(CellSelection& sel) const;

	private:
		static constexpr uint32_t kPoolSlots = QueryParser::kMaxDepth + 4;

		bool				EvalNode(const QueryParser& parser, const QueryResolver& resolver, LayerReader& reader, uint32_t node, uint32_t slot);
		bool				EvalCompare(const QueryNode& node, const QueryBinding& binding, LayerReader& reader, uint64_t* out);
		uint64_t*			Slot(uint32_t slot) { return mPool + static_cast<size_t>(slot) * WordCount(); }
		bool				FailNoData(const QueryNode& node);

		uint64_t*			mPool = nullptr;
		uint32_t			mPoolWords = 0;		// words per slot the pool was allocated for
		uint32_t			mCellCount = 0;
		bool				mHasResult = false;
		QueryError			mError;
	};
}
