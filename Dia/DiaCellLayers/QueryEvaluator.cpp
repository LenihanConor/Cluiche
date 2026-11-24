#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <math.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace Dia::CellLayers
{
	namespace
	{
		inline uint32_t PopCount64(uint64_t v)
		{
#if defined(_MSC_VER) && defined(_M_X64)
			return static_cast<uint32_t>(__popcnt64(v));
#else
			uint32_t n = 0;
			while (v != 0) { v &= v - 1; ++n; }
			return n;
#endif
		}

		inline bool CompareFloat(float v, QueryCompareOp op, float t)
		{
			if (v != v) { return false; }	// NaN: false for every operator including !=
			switch (op)
			{
			case QueryCompareOp::Lt: return v < t;
			case QueryCompareOp::Le: return v <= t;
			case QueryCompareOp::Gt: return v > t;
			case QueryCompareOp::Ge: return v >= t;
			case QueryCompareOp::Eq: return v == t;
			case QueryCompareOp::Ne: return v != t;
			}
			return false;
		}

		inline bool CompareCategory(float v, QueryCompareOp op, float t)
		{
			if (v != v) { return false; }
			const bool eq = floorf(v + 0.5f) == t;
			return op == QueryCompareOp::Ne ? !eq : eq;
		}
	}

	QueryEvaluator::~QueryEvaluator()
	{
		delete[] mPool;
	}

	bool QueryEvaluator::FailNoData(const QueryNode& node)
	{
		mError = QueryError();
		strncpy_s(mError.mMessage, QueryError::kMessageSize, "no data for layer", _TRUNCATE);
		mError.mColumn = node.mName.mColumn;
		mError.mLength = node.mName.mLength;
		return false;
	}

	bool QueryEvaluator::Evaluate(const QueryParser& parser, const QueryResolver& resolver, LayerReader& reader, uint32_t cellCount)
	{
		DIA_PROFILE_SCOPE("celllayers.query.evaluate", ::Dia::Observation::Profile::Category::kDiaGraphics);
		mError = QueryError();
		mHasResult = false;
		mCellCount = cellCount;

		const uint32_t words = WordCount();
		if (parser.RootIndex() == QueryNode::kNone || words == 0)
		{
			strncpy_s(mError.mMessage, QueryError::kMessageSize, "nothing to evaluate", _TRUNCATE);
			return false;
		}
		if (mPool == nullptr || mPoolWords != words)
		{
			delete[] mPool;
			mPool = new uint64_t[static_cast<size_t>(words) * kPoolSlots];
			mPoolWords = words;
		}
		if (!EvalNode(parser, resolver, reader, parser.RootIndex(), 0))
		{
			return false;
		}
		mHasResult = true;
		return true;
	}

	bool QueryEvaluator::EvalNode(const QueryParser& parser, const QueryResolver& resolver, LayerReader& reader, uint32_t nodeIndex, uint32_t slot)
	{
		const QueryNode& node = parser.Node(nodeIndex);
		uint64_t* out = Slot(slot);
		const uint32_t words = WordCount();

		switch (node.mKind)
		{
		case QueryNodeKind::Compare:
			return EvalCompare(node, resolver.Binding(nodeIndex), reader, out);

		case QueryNodeKind::Not:
		{
			if (!EvalNode(parser, resolver, reader, node.mLeft, slot)) { return false; }
			for (uint32_t w = 0; w < words; ++w) { out[w] = ~out[w]; }
			const uint32_t tail = mCellCount % 64u;
			if (tail != 0) { out[words - 1] &= (uint64_t(1) << tail) - 1u; }
			return true;
		}

		case QueryNodeKind::And:
		case QueryNodeKind::Or:
		{
			if (slot + 1 >= kPoolSlots) { return false; }
			if (!EvalNode(parser, resolver, reader, node.mLeft, slot)) { return false; }
			if (!EvalNode(parser, resolver, reader, node.mRight, slot + 1)) { return false; }
			const uint64_t* rhs = Slot(slot + 1);
			if (node.mKind == QueryNodeKind::And) { for (uint32_t w = 0; w < words; ++w) { out[w] &= rhs[w]; } }
			else { for (uint32_t w = 0; w < words; ++w) { out[w] |= rhs[w]; } }
			return true;
		}
		}
		return false;
	}

	bool QueryEvaluator::EvalCompare(const QueryNode& node, const QueryBinding& binding, LayerReader& reader, uint64_t* out)
	{
		const LayerValues lv = reader.Read(binding.mLayerId);
		if (!binding.mBound || !lv.hasData || lv.values == nullptr || lv.count != mCellCount)
		{
			return FailNoData(node);
		}
		memset(out, 0, sizeof(uint64_t) * WordCount());
		const float t = static_cast<float>(binding.mThreshold);
		const QueryCompareOp op = binding.mOp;
		const float* v = lv.values;
		if (binding.mKind == CellValueKind::Category)
		{
			for (uint32_t i = 0; i < mCellCount; ++i)
			{
				if (CompareCategory(v[i], op, t)) { out[i >> 6] |= uint64_t(1) << (i & 63u); }
			}
		}
		else
		{
			for (uint32_t i = 0; i < mCellCount; ++i)
			{
				if (CompareFloat(v[i], op, t)) { out[i >> 6] |= uint64_t(1) << (i & 63u); }
			}
		}
		return true;
	}

	bool QueryEvaluator::Test(uint32_t cell) const
	{
		if (!mHasResult || cell >= mCellCount) { return false; }
		return ((mPool[cell >> 6] >> (cell & 63u)) & 1u) != 0;
	}

	uint32_t QueryEvaluator::Count() const
	{
		if (!mHasResult) { return 0; }
		uint32_t n = 0;
		const uint32_t words = WordCount();
		for (uint32_t w = 0; w < words; ++w) { n += PopCount64(mPool[w]); }
		return n;
	}

	bool QueryEvaluator::ApplyTo(CellSelection& sel) const
	{
		if (!mHasResult || sel.CellCount() != mCellCount) { return false; }
		sel.BeginEdit();
		for (uint32_t i = 0; i < mCellCount; ++i) { sel.Set(i, Test(i)); }
		sel.EndEdit();
		return true;
	}
}
