#pragma once

// HistoryConfig: the per-testbed "history" block of a .diastage, plus the DiaValueSchema history-flag helpers.
// Fixed capacity, no STL. Every key is optional; ParseHistoryConfig is all-or-nothing (a rejected block leaves
// the output untouched).

#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>
#include <json/json.h>
#include <cstdint>

namespace Dia::ValueSchema { struct ValueInfo; }

namespace Dia::CellHistory
{
	struct HistoryConfig
	{
		static constexpr uint32_t kMaxBands = 16;
		static constexpr uint32_t kMaxTrack = 64;
		static constexpr uint32_t kHardMaxAggregateLayers = 64;
		static constexpr uint32_t kBandNameLength = 31;		// excluding terminator
		static constexpr uint32_t kLayerNameLength = 63;
		static constexpr uint32_t kGridNameLength = 63;

		Dia::Core::Containers::String32	mBands[kMaxBands];			// empty list = module default bands
		uint32_t			mBandCount = 0;
		Dia::Core::Containers::String64	mTrack[kMaxTrack];			// layer ids tracked in addition to schema history.track
		uint32_t			mTrackCount = 0;
		Dia::Core::Containers::String64	mGrid;						// empty = grid of the first registered layer
		uint32_t			mCellBudgetMB = 64;
		uint32_t			mAggregateBudgetMB = 64;
		uint32_t			mEventBudgetMB = 16;
		uint32_t			mMaxAggregateLayers = 64;	// 1..kHardMaxAggregateLayers
	};

	struct HistoryConfigResult
	{
		bool				mOk = true;
		Dia::Core::Containers::String256 mError;	// names the offending key when !mOk
	};

	// json must be an object. Unknown keys log one Warning ("HIS") each and are ignored.
	HistoryConfigResult ParseHistoryConfig(const Json::Value& json, HistoryConfig& out);

	// Schema flags read from ValueInfo::mHistory.
	bool HistoryTrack(const Dia::ValueSchema::ValueInfo& info);		// default false
	bool HistoryGroup(const Dia::ValueSchema::ValueInfo& info);		// true for `true` or a non-empty group name; default false
	bool HistoryAggregate(const Dia::ValueSchema::ValueInfo& info);	// default TRUE when absent (or not a bool)
}
