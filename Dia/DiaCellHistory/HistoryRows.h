#pragma once
#ifdef DIA_DEBUG

// HistoryRows: row builders shared by the catch-up queries (HistoryQueries) and the live topics (HistoryTopics), so both emit the
// file's field names and encodings from ONE implementation. Internal to DiaCellHistory (not part of the public API).
// Same-PU contract: call on the recorder's PU.

#include <DiaCellHistory/AggregateStore.h>
#include <DiaCellHistory/EventLog.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/ValueInfo.h>

#include <json/json.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	namespace Rows
	{
		std::string B64F32(const float* v, uint32_t n);
		std::string B64U16(const uint16_t* v, uint32_t n);

		const Dia::ValueSchema::ValueInfo* SchemaFor(const CellHistoryRecorder& rec, Dia::Core::StringCRC layer);
		// First sample in [aggFirst, count) that has a row for the layer (count when none), as the file's `first`.
		uint32_t FirstRowSample(const CellHistoryRecorder& rec, Dia::Core::StringCRC layer, AggregateKind kind, uint32_t enumCount, uint32_t aggFirst, uint32_t count);
		Json::Value EnumValues(const CellHistoryRecorder& rec, Dia::Core::StringCRC layer, uint32_t enumCount);

		// A recorded layer as an aggregates reply describes it.
		struct AggLayer
		{
			Dia::Core::StringCRC	id;
			bool					isFloat = true;
			uint32_t				enumCount = 0;
			uint32_t				first = 0;
			Json::Value				values;		// category enum ids
		};
		// Layers in recorded order; `wanted` (one flag per recorded layer) or null for all.
		void CollectAggLayers(const CellHistoryRecorder& rec, const std::vector<bool>* wanted, uint32_t aggFirst, uint32_t count, std::vector<AggLayer>& out);
		// { samples, groupSizes, rows } for samples [from, from+n). fill=false uses placeholder text of the exact encoded length
		// (cheap sizing; same serialised size).
		Json::Value BuildAggregateBody(const CellHistoryRecorder& rec, const std::vector<AggLayer>& layers, uint32_t from, uint32_t n, bool fill);

		struct GroupInfo { std::string id; uint32_t layerCrc; uint32_t value; };
		struct TypeInfo { uint32_t crc; std::string key; };
		struct EventContext
		{
			std::vector<GroupInfo>	groups;
			std::vector<TypeInfo>	types;
			void Collect(const CellHistoryRecorder& rec);
		};
		// One event in the file's shape { sample, type, group, cell, payload, text? }. An unresolved type reads `crc:%08x`.
		Json::Value EventToJson(const EventView& e, const EventContext& ctx);
	}
}

#endif // DIA_DEBUG
