#include <DiaCellHistory/GroupDeltaTable.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <algorithm>
#include <cmath>

namespace Dia::CellHistory
{
	namespace
	{
		// True when x ranks before y: larger |change| first, then smaller groupValue, then smaller groupLayerIndex.
		bool RanksBefore(const GroupDeltaRow& x, const GroupDeltaRow& y)
		{
			const double ax = std::fabs(x.change);
			const double ay = std::fabs(y.change);
			if (ax != ay) { return ax > ay; }
			if (x.groupValue != y.groupValue) { return x.groupValue < y.groupValue; }
			return x.groupLayerIndex < y.groupLayerIndex;
		}

		void AppendRow(GroupDeltaTable& t, const GroupDeltaRow& row)
		{
			if (t.rowCount < GroupDeltaTable::kMaxRows)
			{
				t.rows[t.rowCount++] = row;
				return;
			}
			t.truncated = true;
			uint32_t worst = 0;
			for (uint32_t i = 1; i < t.rowCount; ++i)
			{
				if (RanksBefore(t.rows[worst], t.rows[i])) { worst = i; }
			}
			if (RanksBefore(row, t.rows[worst])) { t.rows[worst] = row; }
		}
	}

	void GroupDeltaTable::Clear()
	{
		rowCount = 0;
		truncated = false;
		baseNoData = false;
		for (uint32_t i = 0; i < kMaxGroupLayers; ++i) { usedLive[i] = false; skipped[i] = false; }
	}

	bool IsGroupValue(float v)
	{
		return std::isfinite(v) && v >= 0.0f && v < float(GroupDeltaTable::kGroupValueCap) && v == std::floor(v);
	}

	void AccumulateGroups(GroupDeltaTable& table, uint32_t groupLayerIndex, const float* a, const float* b, const float* groupValues, uint32_t n, const uint8_t* selectedMask)
	{
		if (a == nullptr || b == nullptr || groupValues == nullptr || n == 0) { return; }
		double sumA[GroupDeltaTable::kGroupValueCap] = {};
		double sumB[GroupDeltaTable::kGroupValueCap] = {};
		uint32_t count[GroupDeltaTable::kGroupValueCap] = {};
		for (uint32_t i = 0; i < n; ++i)
		{
			if (selectedMask != nullptr && selectedMask[i] == 0) { continue; }
			if (!IsGroupValue(groupValues[i])) { continue; }
			if (std::isnan(a[i]) || std::isnan(b[i])) { continue; }
			const uint32_t g = static_cast<uint32_t>(groupValues[i]);
			sumA[g] += a[i];
			sumB[g] += b[i];
			++count[g];
		}
		for (uint32_t g = 0; g < GroupDeltaTable::kGroupValueCap; ++g)
		{
			if (count[g] == 0) { continue; }
			GroupDeltaRow row;
			row.groupLayerIndex = groupLayerIndex;
			row.groupValue = g;
			row.count = count[g];
			row.meanA = sumA[g] / count[g];
			row.meanB = sumB[g] / count[g];
			row.change = row.meanB - row.meanA;
			AppendRow(table, row);
		}
	}

	void SortByAbsChange(GroupDeltaTable& table)
	{
		std::sort(table.rows, table.rows + table.rowCount, RanksBefore);
	}

	GroupDeltaBuilder::GroupDeltaBuilder(const CellHistoryRecorder& recorder) : mRecorder(recorder), mReader(recorder) {}

	GroupDeltaBuilder::~GroupDeltaBuilder()
	{
		delete[] mBufA;
		delete[] mBufB;
		delete[] mBufG;
	}

	void GroupDeltaBuilder::Build(const Dia::CellLayers::CellLayerDesc& base, const DeltaSpec& delta, const Dia::CellLayers::CellLayerDesc* groupLayers,
		uint32_t groupLayerCount, const uint8_t* selectedMask, GroupDeltaTable& out)
	{
		DIA_PROFILE_SCOPE("cellhistory.group_table", ::Dia::Observation::Profile::Category::kDiaGraphics);
		out.Clear();
		const uint32_t n = mRecorder.CellCount();
		if (n == 0) { return; }
		if (mBufCount != n)
		{
			delete[] mBufA; delete[] mBufB; delete[] mBufG;
			mBufA = new float[n];
			mBufB = new float[n];
			mBufG = new float[n];
			mBufCount = n;
		}
		if (mReader.Read(delta.a, base, mBufA, n) != SideReadStatus::Ok || mReader.Read(delta.b, base, mBufB, n) != SideReadStatus::Ok)
		{
			out.baseNoData = true;
			return;
		}
		const Side& groupSide = delta.groupSideB ? delta.b : delta.a;
		const uint32_t layers = groupLayerCount < GroupDeltaTable::kMaxGroupLayers ? groupLayerCount : GroupDeltaTable::kMaxGroupLayers;
		for (uint32_t i = 0; groupLayers != nullptr && i < layers; ++i)
		{
			const Dia::CellLayers::CellLayerDesc& g = groupLayers[i];
			if (mReader.Read(groupSide, g, mBufG, n) != SideReadStatus::Ok)
			{
				if (!g.read) { out.skipped[i] = true; continue; }
				g.read(mBufG, n);
				out.usedLive[i] = true;
				if (!mWarned)
				{
					mWarned = true;
					DIA_LOG_WARNING("HIS", "cell history group table: grouping layer has no data on the chosen side; using live values");
				}
			}
			AccumulateGroups(out, i, mBufA, mBufB, mBufG, n, selectedMask);
		}
		SortByAbsChange(out);
	}
}
