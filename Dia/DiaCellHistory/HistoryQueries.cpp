#ifdef DIA_DEBUG

#include <DiaCellHistory/HistoryQueries.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/HistoryRows.h>
#include <DiaCellHistory/QueryObservation.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <json/json.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace Dia::CellHistory
{
	using Dia::API::ValueParam;
	using Dia::Core::StringCRC;
	using Rows::B64U16;
	using Rows::SchemaFor;
	using Rows::FirstRowSample;
	using Rows::EnumValues;

	namespace
	{
		const char* const kMetaName = "cells.history.meta";
		const char* const kAggName = "cells.history.aggregates";
		const char* const kEventsName = "cells.history.events";
		const char* const kCellsName = "cells.history.cells";
		const ValueParam kParamsMeta[] = { { "part", "string", false, "core (default), groups, layers or schema" }, { "offset", "int", false, "Item to resume at (from next)" } };
		const ValueParam kParamsAgg[] = { { "from", "int", false, "First sample (default: oldest retained)" }, { "to", "int", false, "End sample, exclusive (default: sample count)" }, { "layers", "array", false, "Layer ids to include (default: all)" },
			{ "groups", "array", false, "Group ids to include, e.g. [\"world\"] (default: all)" }, { "stride", "int", false, "Window size in recorded samples; 1 = no re-aggregation (default 1)" },
			{ "layerOffset", "int", false, "Layer index to resume at (from next)" }, { "structure", "string", false, "Structure hash from the reply (from next); rejects a stale cursor" } };
		const ValueParam kParamsEvents[] = { { "from", "int", false, "First sample" }, { "to", "int", false, "End sample, exclusive" }, { "types", "array", false, "Event type names" }, { "skip", "int", false, "Events of sample 'from' already delivered" } };
		const ValueParam kParamsCells[] = { { "layer", "string", true, "Tracked layer id" }, { "from", "int", false, "First sample" }, { "to", "int", false, "End sample, exclusive" }, { "cells", "array", false, "Cell indices (default: all)" }, { "cellOffset", "int", false, "Column to resume at (from next)" } };

		Dia::DebugServer::SimHelpInfo Help(const char* description, const ValueParam* params, unsigned int count)
		{
			Dia::DebugServer::SimHelpInfo h;
			h.description = description; h.category = "cells.history"; h.owner = "DiaCellHistory";
			h.params = params; h.paramCount = count;
			return h;
		}

		// Compact serialised size of a value: the measure the 16 KB host limit applies to.
		size_t JsonSize(const Json::Value& v)
		{
			Json::StreamWriterBuilder b;
			b["indentation"] = "";
			return Json::writeString(b, v).size();
		}

		// Validated catch-up request fields of cells.history.aggregates.
		struct AggRequest
		{
			std::vector<bool> groupWanted;
			bool hasGroups = false;
			uint32_t stride = 1;
			uint32_t layerOffset = 0;
		};

		bool ParseUInt(const Json::Value& v, uint32_t& out)
		{
			if (v.isUInt64()) { if (v.asUInt64() > 0xFFFFFFFFull) { return false; } out = uint32_t(v.asUInt64()); return true; }
			if (v.isInt64()) { const Json::Int64 x = v.asInt64(); if (x < 0 || x > 0xFFFFFFFFll) { return false; } out = uint32_t(x); return true; }
			return false;
		}

		bool NormaliseArgs(const Json::Value& in, Json::Value& out, char* err, size_t errSize)
		{
			if (in.isNull()) { out = Json::Value(Json::objectValue); return true; }
			if (!in.isObject()) { snprintf(err, errSize, "arguments must be an object"); return false; }
			out = in;
			return true;
		}

		std::string B64U8(const uint8_t* v, uint32_t n)
		{
			std::string s(size_t(EncodedLength(n)) + 1, '\0');
			uint32_t len = 0;
			if (!EncodeUInt8(v, n, &s[0], uint32_t(s.size()), len)) { len = 0; }
			s.resize(len);
			return s;
		}

		// Largest n in [1, maxN] with sizeOf(n) <= budget; 0 when even n = 1 does not fit. sizeOf must be monotonic.
		template <class SizeFn>
		uint32_t FitCount(uint32_t maxN, size_t budget, SizeFn sizeOf)
		{
			if (maxN == 0 || sizeOf(1) > budget) { return 0; }
			uint32_t lo = 1, hi = 1;
			for (;;)
			{
				if (lo >= maxN) { return maxN; }
				hi = (lo > maxN / 2) ? maxN : lo * 2;
				if (sizeOf(hi) <= budget) { lo = hi; } else { break; }
			}
			while (hi - lo > 1)
			{
				const uint32_t mid = lo + (hi - lo) / 2;
				if (sizeOf(mid) <= budget) { lo = mid; } else { hi = mid; }
			}
			return lo;
		}

		Json::Value Cursor(const char* part, uint32_t offset)
		{
			Json::Value c(Json::objectValue);
			c["part"] = part;
			c["offset"] = offset;
			return c;
		}

		// Number of items from `start` that fit after `skeleton` (which already holds empty item arrays); items are costed
		// by their serialised size plus a comma. Reserves room for the longest `next` cursor.
		uint32_t FillItems(const Json::Value& skeleton, const std::vector<Json::Value>& items, uint32_t start, size_t budget)
		{
			Json::Value probe = skeleton;
			probe["next"] = Cursor("schema", 0xFFFFFFFFu);
			size_t used = JsonSize(probe);
			uint32_t i = start;
			for (; i < items.size(); ++i)
			{
				const size_t s = JsonSize(items[i]) + 1;
				if (used + s > budget) { break; }
				used += s;
			}
			return i;
		}

		StringCRC SchemaKeyFor(const CellHistoryRecorder& rec, StringCRC layer)
		{
			const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(layer);
			return desc != nullptr ? desc->schemaKey : layer;
		}

		template <class Store>
		Json::Value GapList(const Store& store, uint32_t count)
		{
			Json::Value list(Json::arrayValue);
			for (uint32_t i = 0; i < count; ++i)
			{
				const auto g = store.Gap(i);
				Json::Value pair(Json::arrayValue);
				pair.append(g.mFirstDropped);
				pair.append(g.mLastDropped);
				list.append(pair);
			}
			return list;
		}

		struct EventScan
		{
			uint32_t						from = 0;
			uint32_t						skip = 0;
			bool							useFilter = false;
			std::vector<uint32_t>			filter;
			const Rows::EventContext*		ctx = nullptr;
			size_t							used = 0;
			size_t							budget = 0;
			uint32_t						maxEvents = 0;
			std::vector<Json::Value>		out;
			uint32_t						cur = 0xFFFFFFFFu;
			uint32_t						index = 0;
			uint32_t						countAtFrom = 0;
			bool							full = false;
			bool							overBudget = false;		// the first event alone did not fit
			uint32_t						nextSample = 0, nextSkip = 0;
		};

		void ScanEvent(const EventView& e, void* context)
		{
			EventScan& c = *static_cast<EventScan*>(context);
			if (e.mSample != c.cur) { c.cur = e.mSample; c.index = 0; }
			const uint32_t myIndex = c.index++;
			if (e.mSample == c.from)
			{
				++c.countAtFrom;
				if (myIndex < c.skip) { return; }
			}
			if (c.full) { return; }
			if (c.useFilter)
			{
				bool match = false;
				for (uint32_t crc : c.filter) { if (crc == e.mType.Value()) { match = true; break; } }
				if (!match) { return; }
			}
			Json::Value item = Rows::EventToJson(e, *c.ctx);
			const size_t s = JsonSize(item) + (c.out.empty() ? 0 : 1);	// a comma between events
			if (c.out.size() >= c.maxEvents || c.used + s > c.budget)
			{
				c.full = true;
				c.nextSample = e.mSample;
				c.nextSkip = myIndex;
				if (c.out.empty()) { c.overBudget = true; }
				return;
			}
			c.used += s;
			c.out.push_back(std::move(item));
		}
	}

	HistoryQueries::HistoryQueries(const CellHistoryRecorder& recorder) : mRecorder(recorder) {}

	void HistoryQueries::SetAppName(const char* app) { snprintf(mApp, sizeof(mApp), "%s", app != nullptr ? app : ""); }
	void HistoryQueries::SetBuildInfo(const char* gitCommit) { snprintf(mGit, sizeof(mGit), "%s", gitCommit != nullptr ? gitCommit : ""); }
	void HistoryQueries::SetMaxReplyBytes(uint32_t bytes) { mMaxReplyBytes = std::min(std::max(bytes, kMinReplyBytes), kMaxReplyBytes); }
	void HistoryQueries::SetMaxEvents(uint32_t events) { mMaxEvents = std::min(std::max(events, 1u), kMaxEventsPerReply); }

	Json::Value HistoryQueries::Fail(const char* query, const char* message) const
	{
		DIA_LOG_WARNING("HIS", "%s rejected: %s", query, message);
		Json::Value out(Json::objectValue);
		out["error"] = message;
		return out;
	}

	// Verifies the serialised size against the budget, counts the reply, and warns once when the budget cut it short.
	Json::Value HistoryQueries::Finish(const char* query, int kind, Json::Value& reply, bool clamped)
	{
		const size_t bytes = JsonSize(reply);
		if (bytes > mMaxReplyBytes)
		{
			char msg[160];
			snprintf(msg, sizeof(msg), "reply too large: %zu bytes exceeds the %u byte limit", bytes, mMaxReplyBytes);
			return Fail(query, msg);
		}
		if (mObs != nullptr) { mObs->OnQuery(static_cast<QueryObservation::Kind>(kind), bytes); }
		if (clamped && !mClampWarned)
		{
			mClampWarned = true;
			DIA_LOG_WARNING("HIS", "%s: reply clamped to the size cap (%u bytes); the client follows `next` (further clamps are not logged)", query, mMaxReplyBytes);
		}
		return reply;
	}

	// ---- meta -----------------------------------------------------------------------------------------------------------

	Json::Value HistoryQueries::Meta(const Json::Value& argsIn)
	{
		DIA_TRACE_ZONE("cellhistory.query_meta", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("cellhistory.query_meta", ::Dia::Observation::Profile::Category::kDiaGraphics);
		Json::Value args;
		char err[160];
		if (!NormaliseArgs(argsIn, args, err, sizeof(err))) { return Fail(kMetaName, err); }
		std::string part = "core";
		if (args.isMember("part"))
		{
			if (!args["part"].isString()) { return Fail(kMetaName, "param 'part' must be a string"); }
			part = args["part"].asString();
		}
		if (part != "core" && part != "groups" && part != "layers" && part != "schema") { return Fail(kMetaName, "param 'part' must be core, groups, layers or schema"); }
		uint32_t offset = 0;
		if (args.isMember("offset") && !ParseUInt(args["offset"], offset)) { return Fail(kMetaName, "param 'offset' must be a non-negative integer"); }
		const CellHistoryRecorder& rec = mRecorder;

		if (part == "schema")
		{
			Json::Value subset(Json::objectValue);
			HistoryExporter::BuildSchema(rec, subset);
			std::vector<Json::Value> items;
			for (const Json::Value& e : subset["entries"]) { items.push_back(e); }
			const uint32_t entryCount = uint32_t(items.size());
			for (const Json::Value& e : subset["ramps"]) { items.push_back(e); }
			const uint32_t total = uint32_t(items.size());
			if (offset > total) { return Fail(kMetaName, "invalid cursor: schema offset is past the last item"); }
			Json::Value reply(Json::objectValue);
			reply["part"] = "schema";
			reply["entries"] = Json::Value(Json::arrayValue);
			reply["ramps"] = Json::Value(Json::arrayValue);
			const uint32_t end = FillItems(reply, items, offset, mMaxReplyBytes);
			if (end == offset && offset < total) { return Fail(kMetaName, "reply too large: one schema item exceeds the reply limit"); }
			for (uint32_t i = offset; i < end; ++i) { reply[i < entryCount ? "entries" : "ramps"].append(items[i]); }
			if (end < total) { reply["next"] = Cursor("schema", end); }
			return Finish(kMetaName, 0, reply, end < total);
		}

		const AggregateStore& agg = rec.Aggregates();
		const uint32_t count = rec.SampleCount();
		const uint32_t aggFirst = agg.Empty() ? count : agg.FirstRetained();

		// Items: groups first, then layers.
		std::vector<Json::Value> items;
		const uint32_t groupCount = agg.GroupCount();
		for (uint32_t g = 0; g < groupCount; ++g)
		{
			const StringCRC gid = agg.GroupAt(g);
			Json::Value it(Json::objectValue);
			it["id"] = gid.AsChar();
			StringCRC layer;
			uint32_t value = 0;
			if (!rec.GroupSource(g, layer, value)) { it["label"] = "World"; items.push_back(it); continue; }
			const Dia::ValueSchema::ValueInfo* info = SchemaFor(rec, layer);
			const char* eq = std::strchr(gid.AsChar(), '=');
			const std::string valueText = eq != nullptr ? std::string(eq + 1) : std::string(gid.AsChar());
			const char* label = valueText.c_str();
			if (info != nullptr && value < info->mEnumValues.Size() && info->mEnumValues[value].mLabel.Length() > 0) { label = info->mEnumValues[value].mLabel.AsCStr(); }
			it["layer"] = layer.AsChar();
			it["value"] = valueText;
			it["label"] = label;
			items.push_back(it);
		}
		for (uint32_t li = 0; li < rec.RecordedLayerCount(); ++li)
		{
			const StringCRC layer = rec.RecordedLayerId(li);
			AggregateKind kind;
			uint32_t enumCount = 0;
			if (!agg.LayerInfo(layer, kind, enumCount)) { continue; }
			Json::Value it(Json::objectValue);
			it["id"] = layer.AsChar();
			it["kind"] = kind == AggregateKind::Float ? "float" : "category";
			it["first"] = FirstRowSample(rec, layer, kind, enumCount, aggFirst, count);
			const StringCRC key = SchemaKeyFor(rec, layer);
			if (key.Value() != layer.Value()) { it["schemaKey"] = key.AsChar(); }
			if (kind == AggregateKind::Category) { it["values"] = EnumValues(rec, layer, enumCount); }
			items.push_back(it);
		}
		const uint32_t G = groupCount;
		const uint32_t total = uint32_t(items.size());
		uint32_t pos = 0;
		if (part == "groups") { if (offset > G) { return Fail(kMetaName, "invalid cursor: groups offset is past the last group"); } pos = offset; }
		if (part == "layers") { if (offset > total - G) { return Fail(kMetaName, "invalid cursor: layers offset is past the last layer"); } pos = G + offset; }

		Json::Value reply(Json::objectValue);
		reply["part"] = part;
		if (part == "core")
		{
			Json::Value h(Json::objectValue);
			if (mApp[0] != '\0') { h["app"] = mApp; }
			if (mGit[0] != '\0') { h["git_commit"] = mGit; }
			h["world"] = rec.WorldId();
			h["sampleCount"] = count;
			h["sampleFirst"] = rec.HeaderFirst();
			h["aggregateFirst"] = aggFirst;
			h["cellFirst"] = rec.FirstRetained();
			reply["header"] = h;
			Json::Value bands(Json::arrayValue);
			for (uint32_t i = 0; i < rec.BandCount(); ++i) { bands.append(rec.BandName(i)); }
			reply["bands"] = bands;
			CellHistoryRecorder::HistoryStatus status;
			rec.BuildStatus(status);
			Json::Value tracked(Json::arrayValue);
			for (uint32_t i = 0; i < status.mTrackedCount; ++i) { tracked.append(status.mTracked[i].AsChar()); }
			reply["tracked"] = tracked;
			Json::Value gaps(Json::objectValue);
			gaps["aggregates"] = GapList(agg, agg.GapCount());
			gaps["cells"] = rec.Ring() != nullptr ? GapList(*rec.Ring(), rec.Ring()->GapCount()) : Json::Value(Json::arrayValue);
			gaps["events"] = GapList(rec.EventsLog(), rec.EventsLog().GapCount());
			reply["gaps"] = gaps;
			char hex[24];
			snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)rec.StructureHash());
			reply["structure"] = hex;
			Json::Value snaps(Json::arrayValue);
			if (mSnapshots != nullptr)
			{
				SnapshotSummary list[64];
				const uint32_t n = mSnapshots->ListSummaries(list, 64);
				for (uint32_t i = 0; i < n; ++i)
				{
					Json::Value s(Json::objectValue);
					s["name"] = list[i].mName; s["sample"] = list[i].mSample; s["label"] = list[i].mLabel;
					snaps.append(s);
				}
			}
			reply["snapshots"] = snaps;
		}
		reply["groups"] = Json::Value(Json::arrayValue);
		reply["layers"] = Json::Value(Json::arrayValue);
		const uint32_t end = FillItems(reply, items, pos, mMaxReplyBytes);
		if (end == pos && pos < total && part != "core") { return Fail(kMetaName, "reply too large: one item exceeds the reply limit"); }
		for (uint32_t i = pos; i < end; ++i) { reply[i < G ? "groups" : "layers"].append(items[i]); }
		if (end < total) { reply["next"] = end < G ? Cursor("groups", end) : Cursor("layers", end - G); }
		return Finish(kMetaName, 0, reply, end < total);
	}

	// ---- aggregates -----------------------------------------------------------------------------------------------------

	Json::Value HistoryQueries::Aggregates(const Json::Value& argsIn)
	{
		DIA_TRACE_ZONE("cellhistory.query_aggregates", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("cellhistory.query_aggregates", ::Dia::Observation::Profile::Category::kDiaGraphics);
		Json::Value args;
		char err[200];
		if (!NormaliseArgs(argsIn, args, err, sizeof(err))) { return Fail(kAggName, err); }
		const CellHistoryRecorder& rec = mRecorder;
		const AggregateStore& agg = rec.Aggregates();
		const uint32_t count = rec.SampleCount();
		const uint32_t aggFirst = agg.Empty() ? count : agg.FirstRetained();
		uint32_t from = aggFirst, to = count;
		if (args.isMember("from") && !ParseUInt(args["from"], from)) { return Fail(kAggName, "param 'from' must be a non-negative integer"); }
		if (args.isMember("to") && !ParseUInt(args["to"], to)) { return Fail(kAggName, "param 'to' must be a non-negative integer"); }
		if (from > to) { snprintf(err, sizeof(err), "param 'from' (%u) is greater than 'to' (%u)", from, to); return Fail(kAggName, err); }
		if (from < aggFirst || to > count)
		{
			snprintf(err, sizeof(err), "range [%u,%u) is outside the retained aggregate window [%u,%u)", from, to, aggFirst, count);
			return Fail(kAggName, err);
		}

		// Layer selection (recorded order).
		std::vector<bool> wanted(rec.RecordedLayerCount(), !args.isMember("layers"));
		if (args.isMember("layers"))
		{
			if (!args["layers"].isArray()) { return Fail(kAggName, "param 'layers' must be an array of layer ids"); }
			for (const Json::Value& v : args["layers"])
			{
				if (!v.isString()) { return Fail(kAggName, "param 'layers' must be an array of layer ids"); }
				const uint32_t crc = StringCRC(v.asString().c_str()).Value();
				bool found = false;
				for (uint32_t li = 0; li < rec.RecordedLayerCount(); ++li) { if (rec.RecordedLayerId(li).Value() == crc) { wanted[li] = true; found = true; break; } }
				if (!found) { snprintf(err, sizeof(err), "unknown layer '%.100s' (not a recorded layer)", v.asString().c_str()); return Fail(kAggName, err); }
			}
		}
		std::vector<Rows::AggLayer> layers;
		Rows::CollectAggLayers(rec, &wanted, aggFirst, count, layers);
		char hex[24];
		snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)rec.StructureHash());

		// Catch-up fields (validated here; applied by later tasks).
		AggRequest req;
		req.groupWanted.assign(agg.GroupCount(), false);
		if (args.isMember("groups"))
		{
			if (!args["groups"].isArray()) { return Fail(kAggName, "param 'groups' must be an array of group ids"); }
			req.hasGroups = true;
			for (const Json::Value& v : args["groups"])
			{
				if (!v.isString()) { return Fail(kAggName, "param 'groups' must be an array of group ids"); }
				const uint32_t crc = StringCRC(v.asString().c_str()).Value();
				bool found = false;
				for (uint32_t g = 0; g < agg.GroupCount(); ++g) { if (agg.GroupAt(g).Value() == crc) { req.groupWanted[g] = true; found = true; break; } }
				if (!found) { snprintf(err, sizeof(err), "unknown group '%.100s'", v.asString().c_str()); return Fail(kAggName, err); }
			}
		}
		if (args.isMember("stride") && (!ParseUInt(args["stride"], req.stride) || req.stride < 1)) { return Fail(kAggName, "param 'stride' must be an integer >= 1"); }
		if (args.isMember("layerOffset"))
		{
			if (!ParseUInt(args["layerOffset"], req.layerOffset)) { return Fail(kAggName, "param 'layerOffset' must be a non-negative integer"); }
			if (req.layerOffset != 0 && req.layerOffset >= layers.size())
			{
				snprintf(err, sizeof(err), "invalid cursor: layerOffset %u is past the last layer", req.layerOffset);
				return Fail(kAggName, err);
			}
		}
		if (args.isMember("structure"))
		{
			if (!args["structure"].isString()) { return Fail(kAggName, "param 'structure' must be a string"); }
			if (args["structure"].asString() != hex) { return Fail(kAggName, "structure changed: restart the catch-up"); }
		}

		// Builds the reply for [from, from+n). fill=false uses placeholder text of the exact encoded length (cheap sizing).
		auto build = [&](uint32_t n, bool fill) -> Json::Value
		{
			Json::Value r = Rows::BuildAggregateBody(rec, layers, from, n, fill, req.hasGroups ? &req.groupWanted : nullptr);
			r["structure"] = hex;
			r["from"] = from;
			r["count"] = n;
			if (from + n < to) { Json::Value nx(Json::objectValue); nx["from"] = from + n; r["next"] = nx; }
			return r;
		};

		if (req.layerOffset > 0 && from >= to) { return Fail(kAggName, "invalid cursor: layerOffset needs a sample inside the range"); }

		if (req.stride > 1)
		{
			if (req.layerOffset > 0) { return Fail(kAggName, "stride with layer slicing is not supported yet"); }	// TODO(task 5)
			const uint32_t stride = req.stride;
			const uint32_t windowsTotal = to > from ? uint32_t((uint64_t(to - from) + stride - 1) / stride) : 0u;
			auto buildStrided = [&](uint32_t w, bool fill) -> Json::Value
			{
				Json::Value r = Rows::BuildStridedAggregateBody(rec, layers, from, w, stride, to, fill, req.hasGroups ? &req.groupWanted : nullptr);
				r["structure"] = hex;
				r["from"] = from;
				r["count"] = w;
				r["stride"] = stride;
				if (w < windowsTotal) { Json::Value nx(Json::objectValue); nx["from"] = from + w * stride; r["next"] = nx; }
				return r;
			};
			uint32_t w = 0;
			if (windowsTotal > 0) { w = FitCount(windowsTotal, mMaxReplyBytes, [&](uint32_t k) { return JsonSize(buildStrided(k, false)); }); }
			if (w == 0 && windowsTotal > 0) { return Fail(kAggName, "stride with layer slicing is not supported yet"); }	// TODO(task 5)
			Json::Value reply = buildStrided(w, true);
			return Finish(kAggName, 1, reply, w < windowsTotal);
		}

		uint32_t n = 0;
		if (to > from && req.layerOffset == 0)
		{
			n = FitCount(to - from, mMaxReplyBytes, [&](uint32_t k) { return JsonSize(build(k, false)); });
		}
		if (n > 0 || (to <= from && req.layerOffset == 0))
		{
			Json::Value reply = build(n, true);
			return Finish(kAggName, 1, reply, from + n < to);
		}

		// Slice path: one sample, layers [layerOffset, layerOffset+k). samples/groupSizes repeat in every slice (one sample: cheap).
		// Sizing always includes a worst-case `next` so the final reply never exceeds the budget.
		const uint32_t total = uint32_t(layers.size());
		const uint32_t offset = req.layerOffset;
		auto buildSlice = [&](uint32_t k, bool fill) -> Json::Value
		{
			const std::vector<Rows::AggLayer> sub(layers.begin() + offset, layers.begin() + offset + k);
			Json::Value r = Rows::BuildAggregateBody(rec, sub, from, 1, fill, req.hasGroups ? &req.groupWanted : nullptr);
			r["structure"] = hex;
			r["from"] = from;
			r["count"] = 1;
			r["layerOffset"] = offset;
			r["layerCount"] = k;
			r["layerTotal"] = total;
			return r;
		};
		auto withNext = [&](Json::Value r, uint32_t nextFrom, uint32_t nextLayer) -> Json::Value
		{
			Json::Value nx(Json::objectValue);
			nx["from"] = nextFrom; nx["layerOffset"] = nextLayer; nx["structure"] = hex;
			r["next"] = nx;
			return r;
		};
		const uint32_t k = FitCount(total - offset, mMaxReplyBytes, [&](uint32_t kk) { return JsonSize(withNext(buildSlice(kk, false), to, total)); });
		if (k == 0) { return Fail(kAggName, "reply too large: request fewer groups (one layer of the requested groups does not fit in one reply)"); }
		Json::Value reply = buildSlice(k, true);
		bool more = false;
		if (offset + k < total) { reply = withNext(reply, from, offset + k); more = true; }
		else if (from + 1 < to) { reply = withNext(reply, from + 1, 0); more = true; }
		return Finish(kAggName, 1, reply, more);
	}

	// ---- events ---------------------------------------------------------------------------------------------------------

	Json::Value HistoryQueries::Events(const Json::Value& argsIn)
	{
		DIA_TRACE_ZONE("cellhistory.query_events", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("cellhistory.query_events", ::Dia::Observation::Profile::Category::kDiaGraphics);
		Json::Value args;
		char err[200];
		if (!NormaliseArgs(argsIn, args, err, sizeof(err))) { return Fail(kEventsName, err); }
		const CellHistoryRecorder& rec = mRecorder;
		const EventLog& log = rec.EventsLog();
		const uint32_t count = rec.SampleCount();
		uint32_t floor = log.FirstRetainedSample();
		for (uint32_t i = 0; i < log.GapCount(); ++i) { floor = std::max(floor, log.Gap(i).mLastDropped + 1u); }
		floor = std::min(floor, count);
		uint32_t from = floor, to = count, skip = 0;
		if (args.isMember("from") && !ParseUInt(args["from"], from)) { return Fail(kEventsName, "param 'from' must be a non-negative integer"); }
		if (args.isMember("to") && !ParseUInt(args["to"], to)) { return Fail(kEventsName, "param 'to' must be a non-negative integer"); }
		if (args.isMember("skip") && !ParseUInt(args["skip"], skip)) { return Fail(kEventsName, "param 'skip' must be a non-negative integer"); }
		if (from > to) { snprintf(err, sizeof(err), "param 'from' (%u) is greater than 'to' (%u)", from, to); return Fail(kEventsName, err); }
		if (to > count || (to > from && !log.CoversSamples(from, to - 1)))
		{
			snprintf(err, sizeof(err), "range [%u,%u) is outside the retained window [%u,%u) (earlier events were evicted)", from, to, floor, count);
			return Fail(kEventsName, err);
		}
		if (skip > 0 && from >= to) { return Fail(kEventsName, "invalid cursor: skip needs a sample inside the range"); }

		std::vector<uint32_t> filter;
		bool useFilter = false;
		if (args.isMember("types"))
		{
			if (!args["types"].isArray()) { return Fail(kEventsName, "param 'types' must be an array of event type names"); }
			useFilter = true;
			for (const Json::Value& v : args["types"])
			{
				if (!v.isString()) { return Fail(kEventsName, "param 'types' must be an array of event type names"); }
				filter.push_back(StringCRC(v.asString().c_str()).Value());
			}
		}
		Rows::EventContext evCtx;
		evCtx.Collect(rec);

		// The reply skeleton (without events) is what the events must fit beside. Whether `next` is present, and how wide it is, is
		// only known after a scan, so: scan without `next`; if everything fit, done. Otherwise rescan reserving the cursor the last scan
		// found until the reserved width equals the real one (or, if the width oscillates, keep the safe result with the most events).
		auto skeletonSize = [&](bool withNext, uint32_t nextSample, uint32_t nextSkip)
		{
			Json::Value skeleton(Json::objectValue);
			skeleton["from"] = from; skeleton["to"] = to;
			skeleton["events"] = Json::Value(Json::arrayValue);
			if (withNext) { Json::Value nx(Json::objectValue); nx["from"] = nextSample; nx["skip"] = nextSkip; skeleton["next"] = nx; }
			return JsonSize(skeleton);
		};
		auto runScan = [&](EventScan& scan, size_t base)
		{
			scan.from = from;
			scan.skip = skip;
			scan.useFilter = useFilter;
			scan.filter = filter;
			scan.ctx = &evCtx;
			scan.budget = mMaxReplyBytes;
			scan.maxEvents = mMaxEvents;
			scan.used = base;
			if (to > from) { rec.Events(from, to - 1).ForEach(&ScanEvent, &scan); }
		};
		EventScan scan;
		runScan(scan, skeletonSize(false, 0, 0));
		if (scan.full)
		{
			EventScan chosen;
			bool have = false;
			uint32_t gs = scan.nextSample, gk = scan.nextSkip;
			for (int pass = 0; pass < 6; ++pass)
			{
				EventScan sc;
				runScan(sc, skeletonSize(true, gs, gk));
				if (!sc.full) { chosen = std::move(sc); have = true; break; }
				const size_t real = skeletonSize(true, sc.nextSample, sc.nextSkip), reserved = skeletonSize(true, gs, gk);
				if (real <= reserved)
				{
					const bool better = !have || sc.out.size() > chosen.out.size();
					const bool exact = real == reserved;
					if (better) { chosen = std::move(sc); have = true; }
					if (exact) { break; }
					gs = better ? chosen.nextSample : gs;
					gk = better ? chosen.nextSkip : gk;
					if (!better) { break; }
				}
				else { gs = sc.nextSample; gk = sc.nextSkip; }
			}
			if (!have) { runScan(chosen, skeletonSize(true, 0xFFFFFFFFu, 0xFFFFFFFFu)); }
			scan = std::move(chosen);
		}
		if (skip > scan.countAtFrom) { snprintf(err, sizeof(err), "invalid cursor: sample %u has only %u events, skip is %u", from, scan.countAtFrom, skip); return Fail(kEventsName, err); }
		if (scan.overBudget) { return Fail(kEventsName, "reply too large: a single event exceeds the reply limit"); }

		Json::Value reply(Json::objectValue);
		reply["from"] = from;
		reply["to"] = to;
		Json::Value events(Json::arrayValue);
		for (Json::Value& e : scan.out) { events.append(std::move(e)); }
		reply["events"] = events;
		if (scan.full)
		{
			Json::Value nx(Json::objectValue);
			nx["from"] = scan.nextSample;
			nx["skip"] = scan.nextSkip;
			reply["next"] = nx;
		}
		return Finish(kEventsName, 2, reply, scan.full);
	}

	// ---- cells ----------------------------------------------------------------------------------------------------------

	Json::Value HistoryQueries::Cells(const Json::Value& argsIn)
	{
		DIA_TRACE_ZONE("cellhistory.query_cells", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("cellhistory.query_cells", ::Dia::Observation::Profile::Category::kDiaGraphics);
		Json::Value args;
		char err[240];
		if (!NormaliseArgs(argsIn, args, err, sizeof(err))) { return Fail(kCellsName, err); }
		if (!args.isMember("layer") || !args["layer"].isString()) { return Fail(kCellsName, "missing or non-string param 'layer'"); }
		const std::string name = args["layer"].asString();
		const StringCRC id(name.c_str());
		const CellHistoryRecorder& rec = mRecorder;
		const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(id);
		if (desc == nullptr) { snprintf(err, sizeof(err), "unknown layer '%.100s'", name.c_str()); return Fail(kCellsName, err); }
		const CellRing* ring = rec.Ring();
		CellRingKind kind = CellRingKind::Float16;
		float mn = 0.f, mx = 0.f;
		uint32_t layerFirst = 0;
		bool pending = false;
		if (ring == nullptr || !ring->IsTracked(id) || !ring->LayerInfo(id, kind, mn, mx, layerFirst, pending))
		{
			snprintf(err, sizeof(err), "layer '%.100s' is not tracked; track it first", name.c_str());
			return Fail(kCellsName, err);
		}
		if (pending || ring->Empty())
		{
			snprintf(err, sizeof(err), "layer '%.100s' has no per-cell data retained yet", name.c_str());
			return Fail(kCellsName, err);
		}
		const bool isFloat = kind == CellRingKind::Float16;
		const uint32_t winFirst = std::max(layerFirst, ring->FirstRetained());
		const uint32_t winEnd = ring->LatestSample() + 1u;
		uint32_t from = winFirst, to = winEnd, cellOffset = 0;
		if (args.isMember("from") && !ParseUInt(args["from"], from)) { return Fail(kCellsName, "param 'from' must be a non-negative integer"); }
		if (args.isMember("to") && !ParseUInt(args["to"], to)) { return Fail(kCellsName, "param 'to' must be a non-negative integer"); }
		if (args.isMember("cellOffset") && !ParseUInt(args["cellOffset"], cellOffset)) { return Fail(kCellsName, "param 'cellOffset' must be a non-negative integer"); }
		if (from > to) { snprintf(err, sizeof(err), "param 'from' (%u) is greater than 'to' (%u)", from, to); return Fail(kCellsName, err); }
		if (from < winFirst || to > winEnd)
		{
			snprintf(err, sizeof(err), "range [%u,%u) is outside the retained window [%u,%u) of layer '%.100s'", from, to, winFirst, winEnd, name.c_str());
			return Fail(kCellsName, err);
		}
		const uint32_t cellCount = ring->CellCount();
		std::vector<uint32_t> list;
		bool useList = false;
		if (args.isMember("cells"))
		{
			if (!args["cells"].isArray() || args["cells"].empty()) { return Fail(kCellsName, "param 'cells' must be a non-empty array of cell indices"); }
			if (args["cells"].size() > kMaxCellList) { snprintf(err, sizeof(err), "too many cells: %u listed, the limit is %u", unsigned(args["cells"].size()), kMaxCellList); return Fail(kCellsName, err); }
			for (const Json::Value& v : args["cells"])
			{
				uint32_t c = 0;
				if (!ParseUInt(v, c)) { return Fail(kCellsName, "param 'cells' must hold non-negative integers"); }
				if (c >= cellCount) { snprintf(err, sizeof(err), "cell index %u is out of range (the grid has %u cells)", c, cellCount); return Fail(kCellsName, err); }
				list.push_back(c);
			}
			useList = true;
		}
		const uint32_t total = useList ? uint32_t(list.size()) : cellCount;
		if (cellOffset >= total) { snprintf(err, sizeof(err), "invalid cursor: cellOffset %u is not below the cell count %u", cellOffset, total); return Fail(kCellsName, err); }
		const uint32_t width = desc->grid != nullptr ? uint32_t(desc->grid->width) : 0u;
		const uint32_t height = desc->grid != nullptr ? uint32_t(desc->grid->height) : 0u;
		const uint32_t bpc = isFloat ? 2u : 1u;

		// Reply for m rows (samples from..from+m) of the columns [cellOffset, cellOffset+k).
		auto build = [&](uint32_t m, uint32_t k, bool fill) -> Json::Value
		{
			Json::Value r(Json::objectValue);
			r["layer"] = name;
			r["kind"] = isFloat ? "float" : "category";
			if (isFloat) { r["min"] = double(mn); r["max"] = double(mx); }
			r["width"] = width; r["height"] = height;
			r["from"] = from; r["to"] = from + m;
			if (useList)
			{
				Json::Value cl(Json::arrayValue);
				for (uint32_t j = 0; j < k; ++j) { cl.append(list[cellOffset + j]); }
				r["cells"] = cl;
			}
			else { r["cells"] = Json::Value(Json::nullValue); }
			r["cellOffset"] = cellOffset; r["cellCount"] = k; r["cellTotal"] = total;
			Json::Value data(Json::arrayValue);
			std::vector<uint16_t> full16(fill && isFloat ? cellCount : 0), pick16(fill ? k : 0);
			std::vector<uint8_t> full8(fill && !isFloat ? cellCount : 0), pick8(fill ? k : 0);
			for (uint32_t i = 0; i < m; ++i)
			{
				if (!fill) { data.append(std::string(EncodedLength(k * bpc), 'A')); continue; }
				const uint32_t s = from + i;
				const bool ok = isFloat ? ring->ReadQuantised(s, id, full16.data(), cellCount * 2u) : ring->ReadQuantised(s, id, full8.data(), cellCount);
				if (!ok) { data.append(std::string()); continue; }
				for (uint32_t j = 0; j < k; ++j)
				{
					const uint32_t idx = useList ? list[cellOffset + j] : cellOffset + j;
					if (isFloat) { pick16[j] = full16[idx]; } else { pick8[j] = full8[idx]; }
				}
				data.append(isFloat ? B64U16(pick16.data(), k) : B64U8(pick8.data(), k));
			}
			r["data"] = data;
			if (cellOffset + k < total) { Json::Value nx(Json::objectValue); nx["from"] = from; nx["cellOffset"] = cellOffset + k; r["next"] = nx; }
			else if (from + m < to) { Json::Value nx(Json::objectValue); nx["from"] = from + m; nx["cellOffset"] = 0u; r["next"] = nx; }
			return r;
		};

		const uint32_t rows = to - from;
		uint32_t m = 0, k = total - cellOffset;
		if (rows > 0)
		{
			if (cellOffset > 0 || JsonSize(build(1, total, false)) > mMaxReplyBytes)
			{
				// One row, a column slice.
				m = 1;
				k = FitCount(total - cellOffset, mMaxReplyBytes, [&](uint32_t kk) { return JsonSize(build(1, kk, false)); });
				if (k == 0) { return Fail(kCellsName, "reply too large: not even one cell fits in one reply"); }
			}
			else
			{
				k = total;
				m = FitCount(rows, mMaxReplyBytes, [&](uint32_t mm) { return JsonSize(build(mm, total, false)); });
			}
		}
		Json::Value reply = build(m, k, true);
		return Finish(kCellsName, 3, reply, reply.isMember("next"));
	}

	bool HistoryQueries::Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (mRegistered) { return true; }
		struct Entry { const char* name; Dia::DebugServer::SimHelpInfo help; Dia::CellLayers::MirrorQueryHandler handler; };
		const Entry entries[kNameCount] = {
			{ kMetaName, Help("Catch-up: header, bands, groups, layers, tracked, gaps, snapshots, structure hash. part=schema pages the embedded schema. Pages of <= 15 KB; follow next.", kParamsMeta, 2u), [this](const Json::Value& a) { return Meta(a); } },
			{ kAggName, Help("Aggregate rows (min/mean/max or category counts, base64) for samples [from,to), optional layers filter. Whole samples per reply (<= 15 KB); follow next. Also groups, stride, layerOffset.", kParamsAgg, 7u), [this](const Json::Value& a) { return Aggregates(a); } },
			{ kEventsName, Help("Events in samples [from,to), optional types filter. At most 2000 and 15 KB per reply; follow next {from, skip}.", kParamsEvents, 4u), [this](const Json::Value& a) { return Events(a); } },
			{ kCellsName, Help("Quantised per-cell series (base64) of a tracked layer for samples [from,to), optional cell list. Replies <= 15 KB; follow next {from, cellOffset}.", kParamsCells, 5u), [this](const Json::Value& a) { return Cells(a); } } };
		uint32_t done = 0;
		for (; done < kNameCount; ++done)
		{
			if (!registrar.Register(entries[done].name, entries[done].help, entries[done].handler))
			{
				DIA_LOG_ERROR("HIS", "history queries: registering %s was rejected", entries[done].name);
				break;
			}
		}
		if (done < kNameCount)
		{
			for (uint32_t i = 0; i < done; ++i) { registrar.Unregister(entries[i].name); }
			return false;
		}
		mRegistered = true;
		return true;
	}

	void HistoryQueries::Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (!mRegistered) { return; }
		registrar.Unregister(kMetaName);
		registrar.Unregister(kAggName);
		registrar.Unregister(kEventsName);
		registrar.Unregister(kCellsName);
		mRegistered = false;
	}
}

#endif // DIA_DEBUG
