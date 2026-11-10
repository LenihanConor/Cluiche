#include <DiaCellHistory/HistoryExporter.h>
#include <DiaCellHistory/AtomicFile.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/CellRing.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaGraphics/CellField/CellGridDesc.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	namespace
	{
		// JSON text writer over a stream: counts bytes, no document tree.
		class Sink
		{
		public:
			explicit Sink(std::ostream& out) : mOut(out) {}
			void Raw(const char* s, size_t n) { mOut.write(s, std::streamsize(n)); mBytes += n; }
			void Raw(const char* s) { Raw(s, std::strlen(s)); }
			void Char(char c) { Raw(&c, 1); }
			void Str(const char* s)
			{
				const std::string q = Json::valueToQuotedString(s != nullptr ? s : "");
				Raw(q.data(), q.size());
			}
			void Key(const char* k) { Str(k); Char(':'); }
			void UInt(uint64_t v) { char b[24]; const int n = snprintf(b, sizeof(b), "%llu", (unsigned long long)v); Raw(b, size_t(n)); }
			void Double(double v)
			{
				Json::StreamWriterBuilder b;
				b["indentation"] = "";
				const std::string s = Json::writeString(b, Json::Value(v));
				Raw(s.data(), s.size());
			}
			bool Good() const { return bool(mOut); }
			uint64_t Bytes() const { return mBytes; }
		private:
			std::ostream&	mOut;
			uint64_t		mBytes = 0;
		};

		// Reusable scratch buffers for one array at a time.
		struct Scratch
		{
			std::vector<char>		text;
			std::vector<float>		f0, f1, f2;
			std::vector<uint16_t>	u16;
			std::vector<uint8_t>	u8;
		};

		void WriteEncoded(Sink& out, Scratch& sc, uint32_t bytes, bool (*encode)(const void*, uint32_t, char*, uint32_t, uint32_t&), const void* data, uint32_t n)
		{
			sc.text.resize(size_t(EncodedLength(bytes)) + 1);
			uint32_t len = 0;
			if (!encode(data, n, sc.text.data(), uint32_t(sc.text.size()), len)) { len = 0; }
			out.Char('"');
			out.Raw(sc.text.data(), len);
			out.Char('"');
		}
		bool EncF32(const void* d, uint32_t n, char* o, uint32_t c, uint32_t& l) { return EncodeFloat32(static_cast<const float*>(d), n, o, c, l); }
		bool EncU16(const void* d, uint32_t n, char* o, uint32_t c, uint32_t& l) { return EncodeUInt16(static_cast<const uint16_t*>(d), n, o, c, l); }
		bool EncU8(const void* d, uint32_t n, char* o, uint32_t c, uint32_t& l) { return EncodeUInt8(static_cast<const uint8_t*>(d), n, o, c, l); }

		void WriteF32(Sink& out, Scratch& sc, const float* v, uint32_t n) { WriteEncoded(out, sc, n * 4u, &EncF32, v, n); }
		void WriteU16(Sink& out, Scratch& sc, const uint16_t* v, uint32_t n) { WriteEncoded(out, sc, n * 2u, &EncU16, v, n); }
		void WriteU8(Sink& out, Scratch& sc, const uint8_t* v, uint32_t n) { WriteEncoded(out, sc, n, &EncU8, v, n); }

		struct GroupEntry
		{
			StringCRC	id;
			uint32_t	layerCrc = 0;
			uint32_t	value = GroupRef::kNoValue;
		};

		struct TypeName { uint32_t crc; std::string key; StringCRC fullKey; };
			struct SeenType { uint32_t crc; bool resolved; };

		struct EventCtx
		{
			Sink*								out;
			bool								first = true;
			const std::vector<GroupEntry>*		groups;
			const std::vector<TypeName>*		types;
			std::vector<SeenType>*				seen;		// distinct event types met, in first-seen order
		};

		void WriteEvent(const EventView& e, void* context)
		{
			EventCtx& c = *static_cast<EventCtx*>(context);
			Sink& out = *c.out;
			if (!c.first) { out.Char(','); }
			c.first = false;
			out.Raw("{\"sample\":");
			out.UInt(e.mSample);
			out.Raw(",\"type\":");
			const unsigned int typeCrc = e.mType.Value();
			const char* typeText = nullptr;
			for (const TypeName& t : *c.types) { if (t.crc == typeCrc) { typeText = t.key.c_str(); break; } }
			{
				bool known = false;
				for (const SeenType& t : *c.seen) { if (t.crc == typeCrc) { known = true; break; } }
				if (!known) { c.seen->push_back({ typeCrc, typeText != nullptr }); }
			}
			char fallback[24];
			if (typeText == nullptr) { snprintf(fallback, sizeof(fallback), "crc:%08x", typeCrc); typeText = fallback; }
			out.Str(typeText);
			out.Raw(",\"group\":");
			const char* groupId = nullptr;
			if (!e.GroupIsNone())
			{
				for (const GroupEntry& g : *c.groups) { if (g.layerCrc == e.mGroupLayer.Value() && g.value == e.mGroupValue) { groupId = g.id.AsChar(); break; } }
			}
			if (groupId != nullptr) { out.Str(groupId); } else { out.Raw("null"); }
			out.Raw(",\"cell\":");
			if (e.mCell == HistoryEvent::kNoCell) { out.Raw("null"); } else { out.UInt(e.mCell); }
			out.Raw(",\"payload\":");
			bool payloadOk = false;
			if (e.mPayloadJson != nullptr && e.mPayloadJson[0] != '\0')
			{
				Json::Value parsed;
				Json::Reader reader;
				if (reader.parse(e.mPayloadJson, parsed, false) && parsed.isObject()) { payloadOk = true; out.Raw(e.mPayloadJson); }
			}
			if (!payloadOk) { out.Raw("null"); }
			if (e.mText != nullptr && e.mText[0] != '\0')
			{
				out.Raw(",\"text\":");
				out.Str(e.mText);
			}
			out.Char('}');
		}

		const Dia::ValueSchema::ValueInfo* SchemaFor(const CellHistoryRecorder& rec, StringCRC layer)
		{
			const Dia::ValueSchema::ValueSchemaService* schema = rec.Schema();
			if (schema == nullptr) { return nullptr; }
			const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(layer);
			return schema->Find(desc != nullptr ? desc->schemaKey : layer);
		}

		StringCRC SchemaKeyFor(const CellHistoryRecorder& rec, StringCRC layer)
		{
			const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(layer);
			return desc != nullptr ? desc->schemaKey : layer;
		}

		template <class Store, class GapT>
		void WriteGapList(Sink& out, const Store& store, uint32_t count)
		{
			out.Char('[');
			for (uint32_t i = 0; i < count; ++i)
			{
				const GapT g = store.Gap(i);
				if (i > 0) { out.Char(','); }
				out.Char('['); out.UInt(g.mFirstDropped); out.Char(','); out.UInt(g.mLastDropped); out.Char(']');
			}
			out.Char(']');
		}

		void WriteCells(Sink& out, Scratch& sc, const CellHistoryRecorder& rec, const CellHistoryRecorder::HistoryStatus& status)
		{
			out.Char('{');
			const CellRing* ring = rec.Ring();
			bool firstLayer = true;
			for (uint32_t t = 0; ring != nullptr && t < status.mTrackedCount; ++t)
			{
				const StringCRC id = status.mTracked[t];
				CellRingKind kind;
				float mn = 0.f, mx = 0.f;
				uint32_t layerFirst = 0;
				bool pending = false;
				if (!ring->LayerInfo(id, kind, mn, mx, layerFirst, pending)) { continue; }
				const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(id);
				const uint32_t cells = ring->CellCount();
				const bool isFloat = kind == CellRingKind::Float16;
				const bool hasData = !pending && !ring->Empty();
				const uint32_t first = hasData ? std::max(layerFirst, ring->FirstRetained()) : rec.SampleCount();
				const uint32_t last = hasData ? ring->LatestSample() : 0;
				const uint32_t count = (hasData && last >= first) ? last - first + 1 : 0;

				if (!firstLayer) { out.Char(','); }
				firstLayer = false;
				out.Key(id.AsChar());
				out.Raw(isFloat ? "{\"kind\":\"float\"" : "{\"kind\":\"category\"");
				if (isFloat)
				{
					out.Raw(",\"min\":"); out.Double(double(mn));
					out.Raw(",\"max\":"); out.Double(double(mx));
				}
				else
				{
					out.Raw(",\"values\":[");
					const Dia::ValueSchema::ValueInfo* info = SchemaFor(rec, id);
					const uint32_t n = info != nullptr ? info->mEnumValues.Size() : 0u;
					for (uint32_t v = 0; v < n; ++v) { if (v > 0) { out.Char(','); } out.Str(info->mEnumValues[v].mId.AsCStr()); }
					out.Char(']');
				}
				out.Raw(",\"width\":"); out.UInt(desc != nullptr && desc->grid != nullptr ? desc->grid->width : 0u);
				out.Raw(",\"height\":"); out.UInt(desc != nullptr && desc->grid != nullptr ? desc->grid->height : 0u);
				out.Raw(",\"first\":"); out.UInt(first);
				out.Raw(",\"count\":"); out.UInt(count);
				out.Raw(",\"data\":[");
				if (isFloat) { sc.u16.resize(cells); } else { sc.u8.resize(cells); }
				for (uint32_t s = first; count > 0 && s <= last; ++s)
				{
					if (s > first) { out.Char(','); }
					bool ok = false;
					if (isFloat) { ok = ring->ReadQuantised(s, id, sc.u16.data(), cells * 2u); }
					else { ok = ring->ReadQuantised(s, id, sc.u8.data(), cells); }
					if (!ok) { out.Raw("\"\""); }
					else if (isFloat) { WriteU16(out, sc, sc.u16.data(), cells); }
					else { WriteU8(out, sc, sc.u8.data(), cells); }
				}
				out.Raw("]}");
			}
			out.Char('}');
		}

		void WriteAggregates(Sink& out, Scratch& sc, const CellHistoryRecorder& rec, uint32_t count, uint32_t aggFirst)
		{
			const AggregateStore& agg = rec.Aggregates();
			const StringCRC world = AggregateStore::WorldGroup();
			const uint32_t groups = agg.GroupCount();
			out.Char('{');
			for (uint32_t li = 0; li < rec.RecordedLayerCount(); ++li)
			{
				const StringCRC layer = rec.RecordedLayerId(li);
				AggregateKind kind;
				uint32_t enumCount = 0;
				if (!agg.LayerInfo(layer, kind, enumCount)) { continue; }
				const bool isFloat = kind == AggregateKind::Float;

				// First sample that has a row for this layer (a layer can start after the window start).
				uint32_t first = count;
				uint16_t probe[AggregateStore::kMaxEnumCount];
				for (uint32_t s = aggFirst; s < count; ++s)
				{
					AggregateStats st;
					if (isFloat ? rec.Aggregate(s, layer, world, st) : rec.CategoryCounts(s, layer, world, probe, enumCount)) { first = s; break; }
				}
				const uint32_t n = count - first;

				if (li > 0) { out.Char(','); }
				out.Key(layer.AsChar());
				out.Raw(isFloat ? "{\"kind\":\"float\"" : "{\"kind\":\"category\"");
				out.Raw(",\"first\":"); out.UInt(first);
				if (!isFloat)
				{
					out.Raw(",\"values\":[");
					const Dia::ValueSchema::ValueInfo* info = SchemaFor(rec, layer);
					for (uint32_t v = 0; v < enumCount; ++v)
					{
						if (v > 0) { out.Char(','); }
						if (info != nullptr && v < info->mEnumValues.Size()) { out.Str(info->mEnumValues[v].mId.AsCStr()); }
						else { char b[12]; snprintf(b, sizeof(b), "%u", v); out.Str(b); }
					}
					out.Char(']');
				}
				out.Raw(",\"groups\":{");
				for (uint32_t g = 0; g < groups; ++g)
				{
					const StringCRC gid = agg.GroupAt(g);
					if (g > 0) { out.Char(','); }
					out.Key(gid.AsChar());
					if (isFloat)
					{
						const float nan = std::numeric_limits<float>::quiet_NaN();
						sc.f0.assign(n, nan); sc.f1.assign(n, nan); sc.f2.assign(n, nan);
						for (uint32_t i = 0; i < n; ++i)
						{
							AggregateStats st;
							if (rec.Aggregate(first + i, layer, gid, st)) { sc.f0[i] = st.mMin; sc.f1[i] = st.mMean; sc.f2[i] = st.mMax; }
						}
						out.Raw("{\"min\":"); WriteF32(out, sc, sc.f0.data(), n);
						out.Raw(",\"mean\":"); WriteF32(out, sc, sc.f1.data(), n);
						out.Raw(",\"max\":"); WriteF32(out, sc, sc.f2.data(), n);
						out.Char('}');
					}
					else
					{
						sc.u16.assign(size_t(n) * enumCount, 0);
						for (uint32_t i = 0; i < n; ++i) { rec.CategoryCounts(first + i, layer, gid, sc.u16.data() + size_t(i) * enumCount, enumCount); }
						WriteU16(out, sc, sc.u16.data(), n * enumCount);
					}
				}
				out.Raw("}}");
			}
			out.Char('}');
		}

		void CollectTypes(const CellHistoryRecorder& rec, std::vector<TypeName>& types)
		{
			if (const Dia::ValueSchema::ValueSchemaService* schema = rec.Schema())
			{
				for (unsigned int i = 0; i < schema->Count(); ++i)
				{
					const Dia::ValueSchema::ValueInfo* v = schema->At(i);
					if (v != nullptr && v->mKind == Dia::ValueSchema::ValueKind::Event) { types.push_back({ StringCRC(v->mKey.AsCStr()).Value(), v->mKey.AsCStr(), StringCRC(v->mFullKey.AsCStr()) }); }
				}
			}
		}

		// The embedded schema: ExportSubset of every recorded layer's key, every grouping layer's key and every event type seen.
		// unresolved (optional) counts event types without an entry; each logs one Warning when warn is set.
		void BuildSchemaJson(const CellHistoryRecorder& rec, const std::vector<SeenType>& seen, const std::vector<TypeName>& types, uint32_t groupCount,
			uint32_t* unresolved, Json::Value& subset, bool warn = true)
		{
			std::vector<StringCRC> keys;
			for (uint32_t li = 0; li < rec.RecordedLayerCount(); ++li) { keys.push_back(SchemaKeyFor(rec, rec.RecordedLayerId(li))); }
			for (uint32_t g = 1; g < groupCount; ++g)
			{
				StringCRC layer;
				uint32_t value = 0;
				if (rec.GroupSource(g, layer, value)) { keys.push_back(SchemaKeyFor(rec, layer)); }
			}
			for (const SeenType& t : seen)
			{
				if (t.resolved)
				{
					for (const TypeName& n : types) { if (n.crc == t.crc) { keys.push_back(n.fullKey); break; } }
					continue;
				}
				if (unresolved != nullptr) { ++*unresolved; }
				if (warn) { DIA_LOG_WARNING("HIS", "export: event type crc:%08x has no schema entry; exported without one", t.crc); }
			}
			if (const Dia::ValueSchema::ValueSchemaService* schema = rec.Schema())
			{
				schema->ExportSubset(keys.data(), unsigned(keys.size()), subset);
			}
			else
			{
				subset["entries"] = Json::Value(Json::arrayValue);
				subset["ramps"] = Json::Value(Json::arrayValue);
			}
		}

		void NoteSeen(const EventView& e, void* context)
		{
			auto* c = static_cast<std::pair<const std::vector<TypeName>*, std::vector<SeenType>*>*>(context);
			const unsigned int crc = e.mType.Value();
			for (const SeenType& t : *c->second) { if (t.crc == crc) { return; } }
			bool resolved = false;
			for (const TypeName& n : *c->first) { if (n.crc == crc) { resolved = true; break; } }
			c->second->push_back({ crc, resolved });
		}


		bool WriteBody(const CellHistoryRecorder& rec, const ExportOptions& opt, Sink& out, ExportResult& result)
		{
			Scratch sc;
			const uint32_t count = rec.SampleCount();
			const AggregateStore& agg = rec.Aggregates();
			const uint32_t aggFirst = agg.Empty() ? count : agg.FirstRetained();
			const uint32_t sampleFirst = rec.HeaderFirst();
			CellHistoryRecorder::HistoryStatus status;
			rec.BuildStatus(status);

			out.Raw("{\"format\":\"dia.cell_history\",\"version\":1,\"header\":{");
			out.Raw("\"app\":"); out.Str(opt.app);
			out.Raw(",\"git_commit\":"); out.Str(opt.gitCommit);
			out.Raw(",\"generated_at\":"); out.Str(opt.generatedAt);
			out.Raw(",\"world\":"); out.Str(opt.world[0] != '\0' ? opt.world : rec.WorldId());
			out.Raw(",\"sampleCount\":"); out.UInt(count);
			out.Raw(",\"sampleFirst\":"); out.UInt(sampleFirst);
			out.Raw(",\"aggregateFirst\":"); out.UInt(aggFirst);
			out.Raw(",\"cellFirst\":"); out.UInt(rec.FirstRetained());
			out.Raw("},\"bands\":[");
			for (uint32_t i = 0; i < rec.BandCount(); ++i) { if (i > 0) { out.Char(','); } out.Str(rec.BandName(i)); }

			out.Raw("],\"samples\":[");
			for (uint32_t s = sampleFirst; s < count; ++s)
			{
				const SampleHeader& h = rec.Header(s);
				if (s > sampleFirst) { out.Char(','); }
				out.Raw("{\"tick\":"); out.UInt(h.mTick);
				out.Raw(",\"label\":"); out.Str(h.mLabel.AsCStr());
				out.Raw(",\"band\":"); out.UInt(h.mBand);
				out.Char('}');
			}

			// Groups.
			const uint32_t groupCount = agg.GroupCount();
			std::vector<GroupEntry> groups;
			out.Raw("],\"groups\":[");
			for (uint32_t g = 0; g < groupCount; ++g)
			{
				const StringCRC gid = agg.GroupAt(g);
				if (g > 0) { out.Char(','); }
				out.Raw("{\"id\":"); out.Str(gid.AsChar());
				StringCRC layer;
				uint32_t value = 0;
				if (!rec.GroupSource(g, layer, value))
				{
					out.Raw(",\"label\":\"World\"}");
					continue;
				}
				const Dia::ValueSchema::ValueInfo* info = SchemaFor(rec, layer);
				const char* eq = std::strchr(gid.AsChar(), '=');
				const std::string valueText = eq != nullptr ? std::string(eq + 1) : std::string(gid.AsChar());
				const char* label = valueText.c_str();
				if (info != nullptr && value < info->mEnumValues.Size() && info->mEnumValues[value].mLabel.Length() > 0) { label = info->mEnumValues[value].mLabel.AsCStr(); }
				out.Raw(",\"layer\":"); out.Str(layer.AsChar());
				out.Raw(",\"value\":"); out.Str(valueText.c_str());
				out.Raw(",\"label\":"); out.Str(label);
				out.Char('}');
				GroupEntry ge;
				ge.id = gid;
				ge.layerCrc = layer.Value();
				ge.value = value;
				groups.push_back(ge);
			}

			// Group sizes: [aggregateFirst, sampleCount), one uint16 per sample (0 where no row).
			out.Raw("],\"groupSizes\":{");
			const uint32_t sizeCount = count - aggFirst;
			for (uint32_t g = 0; g < groupCount; ++g)
			{
				const StringCRC gid = agg.GroupAt(g);
				if (g > 0) { out.Char(','); }
				out.Key(gid.AsChar());
				sc.u16.assign(sizeCount, 0);
				for (uint32_t i = 0; i < sizeCount; ++i) { uint16_t v = 0; if (agg.GroupSize(aggFirst + i, gid, v)) { sc.u16[i] = v; } }
				WriteU16(out, sc, sc.u16.data(), sizeCount);
			}

			out.Raw("},\"aggregates\":");
			WriteAggregates(out, sc, rec, count, aggFirst);

			// Events. Type names come from the schema's `kind: event` entries (the log keeps only the CRC).
			std::vector<TypeName> types;
			CollectTypes(rec, types);
			out.Raw(",\"events\":[");
			std::vector<SeenType> seen;
			if (count > 0)
			{
				EventCtx ctx;
				ctx.seen = &seen;
				ctx.out = &out;
				ctx.groups = &groups;
				ctx.types = &types;
				rec.Events(0, count - 1).ForEach(&WriteEvent, &ctx);
			}

			out.Raw("],\"snapshots\":[");
			if (opt.snapshots != nullptr)
			{
				SnapshotSummary list[64];
				const uint32_t n = opt.snapshots->ListSummaries(list, 64);
				for (uint32_t i = 0; i < n; ++i)
				{
					if (i > 0) { out.Char(','); }
					out.Raw("{\"name\":"); out.Str(list[i].mName);
					out.Raw(",\"sample\":"); out.UInt(list[i].mSample);
					out.Raw(",\"label\":"); out.Str(list[i].mLabel);
					out.Char('}');
				}
			}

			out.Raw("],\"tracked\":[");
			for (uint32_t i = 0; i < status.mTrackedCount; ++i) { if (i > 0) { out.Char(','); } out.Str(status.mTracked[i].AsChar()); }

			out.Raw("],\"gaps\":{\"aggregates\":");
			WriteGapList<AggregateStore, AggregateGap>(out, agg, agg.GapCount());
			out.Raw(",\"cells\":");
			if (rec.Ring() != nullptr) { WriteGapList<CellRing, CellRingGap>(out, *rec.Ring(), rec.Ring()->GapCount()); } else { out.Raw("[]"); }
			out.Raw(",\"events\":");
			WriteGapList<EventLog, EventGap>(out, rec.EventsLog(), rec.EventsLog().GapCount());
			out.Char('}');

			if (opt.cells)
			{
				out.Raw(",\"cells\":");
				WriteCells(out, sc, rec, status);
			}

			// Embedded schema: recorded layers, grouping layers and the event types present (one Warning per unresolved type).
			out.Raw(",\"schema\":");
			Json::Value subset(Json::objectValue);
			BuildSchemaJson(rec, seen, types, groupCount, &result.unresolvedEventTypes, subset);
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "";
			const std::string schemaText = Json::writeString(builder, subset);
			out.Raw(schemaText.data(), schemaText.size());
			out.Raw("}\n");

			result.samples = count;
			return out.Good();
		}


		void SetError(ExportResult& r, const char* fmt, const char* a)
		{
			r.ok = false;
			snprintf(r.error, sizeof(r.error), fmt, a != nullptr ? a : "");
		}

		// The one place the file is written: stream into <path>.tmp, then rename over the final name. A failed write removes
		// the temp and leaves nothing under the final name; an existing file is refused (checked first) unless overwrite.
		bool WriteToPath(const CellHistoryRecorder& rec, const ExportOptions& opt, const char* path, ExportResult& result)
		{
			std::error_code ec;
			if (!opt.overwrite && std::filesystem::exists(path, ec))
			{
				SetError(result, "file exists; use --overwrite%s", "");
				DIA_LOG_WARNING("HIS", "export refused: '%s' already exists (use overwrite)", path);
				return false;
			}
			char tmp[1100];
			if (!AtomicFile::TempPath(path, tmp, sizeof(tmp))) { SetError(result, "path too long '%s'", path); return false; }
			{
				std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
				if (!f)
				{
					SetError(result, "cannot create temporary file '%s'", tmp);
					DIA_LOG_ERROR("HIS", "export failed: %s", result.error);
					return false;
				}
				Sink sink(f);
				const bool bodyOk = WriteBody(rec, opt, sink, result);
				f.flush();
				if (!bodyOk || !f)
				{
					f.close();
					AtomicFile::Discard(tmp);
					SetError(result, "write failed for '%s' (disk full or not writable)", tmp);
					DIA_LOG_ERROR("HIS", "export failed: %s (temporary file removed)", result.error);
					return false;
				}
				result.bytes = sink.Bytes();
			}
			char err[160];
			if (!AtomicFile::Commit(tmp, path, err, sizeof(err)))
			{
				SetError(result, "%s", err);
				DIA_LOG_ERROR("HIS", "export failed: %s (temporary file removed)", result.error);
				return false;
			}
			return true;
		}
	}

	bool HistoryExporter::ResolvePath(const char* path, const char* app, const char* outDir, char* buf, uint32_t bufCapacity, char* err, uint32_t errCapacity)
	{
		auto fail = [&](const char* why) { if (err != nullptr && errCapacity > 0) { snprintf(err, errCapacity, "%s", why); } return false; };
		if (buf == nullptr || bufCapacity == 0) { return fail("no output buffer"); }
		buf[0] = '\0';
		if (path == nullptr || path[0] == '\0') { return fail("no path given"); }
		int n = 0;
		if (std::filesystem::path(path).has_root_path())
		{
			n = snprintf(buf, bufCapacity, "%s", path);
		}
		else
		{
			// No `..` segment: a relative path stays inside the history folder.
			const char* p = path;
			while (*p != '\0')
			{
				const char* e = p;
				while (*e != '\0' && *e != '/' && *e != '\\') { ++e; }
				if (e - p == 2 && p[0] == '.' && p[1] == '.') { return fail("relative path must not contain '..'"); }
				p = (*e == '\0') ? e : e + 1;
			}
			if (outDir == nullptr || outDir[0] == '\0') { return fail("no output folder configured for relative paths"); }
			if (app != nullptr && app[0] != '\0') { n = snprintf(buf, bufCapacity, "%s/%s/history/%s", outDir, app, path); }
			else { n = snprintf(buf, bufCapacity, "%s/history/%s", outDir, path); }
		}
		if (n < 0 || uint32_t(n) >= bufCapacity) { buf[0] = '\0'; return fail("resolved path is too long"); }
		return true;
	}


	bool HistoryExporter::BuildSchema(const CellHistoryRecorder& recorder, Json::Value& out)
	{
		std::vector<TypeName> types;
		CollectTypes(recorder, types);
		std::vector<SeenType> seen;
		const uint32_t count = recorder.SampleCount();
		if (count > 0)
		{
			std::pair<const std::vector<TypeName>*, std::vector<SeenType>*> ctx(&types, &seen);
			recorder.Events(0, count - 1).ForEach(&NoteSeen, &ctx);
		}
		out = Json::Value(Json::objectValue);
		BuildSchemaJson(recorder, seen, types, recorder.Aggregates().GroupCount(), nullptr, out, false);
		return true;
	}

	bool HistoryExporter::WriteFile(const CellHistoryRecorder& recorder, const ExportOptions& options, const char* path, ExportResult& result)
	{
		DIA_TRACE_ZONE("cellhistory.export", ::Dia::Observation::Trace::Category::kDiaGraphics);
		DIA_PROFILE_SCOPE("cellhistory.export", ::Dia::Observation::Profile::Category::kDiaGraphics);
		result = ExportResult();
		if (path == nullptr || path[0] == '\0') { SetError(result, "no path given%s", ""); return false; }
		ExportOptions opt = options;
		if (opt.app == nullptr) { opt.app = ""; }
		if (opt.gitCommit == nullptr) { opt.gitCommit = ""; }
		if (opt.generatedAt == nullptr) { opt.generatedAt = ""; }
		if (opt.world == nullptr) { opt.world = ""; }
		if (!WriteToPath(recorder, opt, path, result)) { return false; }
		result.ok = true;
		DIA_LOG_INFO("HIS", "export written (%s, %llu bytes, %u samples)", path, (unsigned long long)result.bytes, result.samples);
		return true;
	}
}
