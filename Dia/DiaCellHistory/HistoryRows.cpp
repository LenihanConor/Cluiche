#ifdef DIA_DEBUG

#include <DiaCellHistory/HistoryRows.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <cstdio>
#include <limits>
#include <memory>

namespace Dia::CellHistory::Rows
{
	using Dia::Core::StringCRC;

	std::string B64F32(const float* v, uint32_t n)
	{
		std::string s(size_t(EncodedLength(n * 4u)) + 1, '\0');
		uint32_t len = 0;
		if (!EncodeFloat32(v, n, &s[0], uint32_t(s.size()), len)) { len = 0; }
		s.resize(len);
		return s;
	}

	std::string B64U16(const uint16_t* v, uint32_t n)
	{
		std::string s(size_t(EncodedLength(n * 2u)) + 1, '\0');
		uint32_t len = 0;
		if (!EncodeUInt16(v, n, &s[0], uint32_t(s.size()), len)) { len = 0; }
		s.resize(len);
		return s;
	}

	const Dia::ValueSchema::ValueInfo* SchemaFor(const CellHistoryRecorder& rec, StringCRC layer)
	{
		const Dia::ValueSchema::ValueSchemaService* schema = rec.Schema();
		if (schema == nullptr) { return nullptr; }
		const Dia::CellLayers::CellLayerDesc* desc = rec.Registry().Find(layer);
		return schema->Find(desc != nullptr ? desc->schemaKey : layer);
	}

	uint32_t FirstRowSample(const CellHistoryRecorder& rec, StringCRC layer, AggregateKind kind, uint32_t enumCount, uint32_t aggFirst, uint32_t count)
	{
		const StringCRC world = AggregateStore::WorldGroup();
		uint16_t probe[AggregateStore::kMaxEnumCount];
		for (uint32_t s = aggFirst; s < count; ++s)
		{
			AggregateStats st;
			if (kind == AggregateKind::Float ? rec.Aggregate(s, layer, world, st) : rec.CategoryCounts(s, layer, world, probe, enumCount)) { return s; }
		}
		return count;
	}

	Json::Value EnumValues(const CellHistoryRecorder& rec, StringCRC layer, uint32_t enumCount)
	{
		Json::Value values(Json::arrayValue);
		const Dia::ValueSchema::ValueInfo* info = SchemaFor(rec, layer);
		for (uint32_t v = 0; v < enumCount; ++v)
		{
			if (info != nullptr && v < info->mEnumValues.Size()) { values.append(info->mEnumValues[v].mId.AsCStr()); }
			else { char b[12]; snprintf(b, sizeof(b), "%u", v); values.append(b); }
		}
		return values;
	}

	void CollectAggLayers(const CellHistoryRecorder& rec, const std::vector<bool>* wanted, uint32_t aggFirst, uint32_t count, std::vector<AggLayer>& out)
	{
		const AggregateStore& agg = rec.Aggregates();
		for (uint32_t li = 0; li < rec.RecordedLayerCount(); ++li)
		{
			if (wanted != nullptr && !(*wanted)[li]) { continue; }
			AggLayer l;
			l.id = rec.RecordedLayerId(li);
			AggregateKind kind;
			if (!agg.LayerInfo(l.id, kind, l.enumCount)) { continue; }
			l.isFloat = kind == AggregateKind::Float;
			l.first = FirstRowSample(rec, l.id, kind, l.enumCount, aggFirst, count);
			if (!l.isFloat) { l.values = EnumValues(rec, l.id, l.enumCount); }
			out.push_back(std::move(l));
		}
	}

	Json::Value BuildAggregateBody(const CellHistoryRecorder& rec, const std::vector<AggLayer>& layers, uint32_t from, uint32_t n, bool fill, const std::vector<bool>* groupWanted)
	{
		const AggregateStore& agg = rec.Aggregates();
		const uint32_t groups = agg.GroupCount();
		Json::Value r(Json::objectValue);
		Json::Value samples(Json::arrayValue);
		for (uint32_t i = 0; i < n; ++i)
		{
			const SampleHeader& h = rec.Header(from + i);
			Json::Value s(Json::objectValue);
			s["tick"] = Json::UInt64(h.mTick); s["label"] = h.mLabel.AsCStr(); s["band"] = unsigned(h.mBand);
			samples.append(s);
		}
		r["samples"] = samples;
		Json::Value sizes(Json::objectValue);
		std::vector<uint16_t> u16;
		std::vector<float> f0, f1, f2;
		for (uint32_t g = 0; g < groups; ++g)
		{
			if (groupWanted && !(*groupWanted)[g]) { continue; }
			const StringCRC gid = agg.GroupAt(g);
			if (!fill) { sizes[gid.AsChar()] = std::string(EncodedLength(n * 2u), 'A'); continue; }
			u16.assign(n, 0);
			for (uint32_t i = 0; i < n; ++i) { uint16_t v = 0; if (agg.GroupSize(from + i, gid, v)) { u16[i] = v; } }
			sizes[gid.AsChar()] = B64U16(u16.data(), n);
		}
		r["groupSizes"] = sizes;
		Json::Value rows(Json::objectValue);
		for (const AggLayer& l : layers)
		{
			Json::Value lr(Json::objectValue);
			lr["kind"] = l.isFloat ? "float" : "category";
			lr["first"] = l.first;
			if (!l.isFloat) { lr["values"] = l.values; }
			Json::Value gs(Json::objectValue);
			for (uint32_t g = 0; g < groups; ++g)
			{
				if (groupWanted && !(*groupWanted)[g]) { continue; }
				const StringCRC gid = agg.GroupAt(g);
				if (l.isFloat)
				{
					Json::Value gv(Json::objectValue);
					if (!fill)
					{
						const std::string ph(EncodedLength(n * 4u), 'A');
						gv["min"] = ph; gv["mean"] = ph; gv["max"] = ph;
					}
					else
					{
						const float nan = std::numeric_limits<float>::quiet_NaN();
						f0.assign(n, nan); f1.assign(n, nan); f2.assign(n, nan);
						for (uint32_t i = 0; i < n; ++i)
						{
							AggregateStats st;
							if (from + i >= l.first && rec.Aggregate(from + i, l.id, gid, st)) { f0[i] = st.mMin; f1[i] = st.mMean; f2[i] = st.mMax; }
						}
						gv["min"] = B64F32(f0.data(), n); gv["mean"] = B64F32(f1.data(), n); gv["max"] = B64F32(f2.data(), n);
					}
					gs[gid.AsChar()] = gv;
				}
				else if (!fill) { gs[gid.AsChar()] = std::string(EncodedLength(n * l.enumCount * 2u), 'A'); }
				else
				{
					u16.assign(size_t(n) * l.enumCount, 0);
					for (uint32_t i = 0; i < n; ++i) { if (from + i >= l.first) { rec.CategoryCounts(from + i, l.id, gid, u16.data() + size_t(i) * l.enumCount, l.enumCount); } }
					gs[gid.AsChar()] = B64U16(u16.data(), uint32_t(u16.size()));
				}
			}
			lr["groups"] = gs;
			rows[l.id.AsChar()] = lr;
		}
		r["rows"] = rows;
		return r;
	}

	WindowStats ReduceFloatWindow(const float* mins, const float* means, const float* maxs, const uint16_t* weights, const bool* valid, uint32_t n)
	{
		const float nan = std::numeric_limits<float>::quiet_NaN();
		WindowStats r{ nan, nan, nan, false };
		bool haveMin = false, haveMax = false;
		double weighted = 0.0, plain = 0.0, totalWeight = 0.0;
		uint32_t meanCount = 0;
		for (uint32_t i = 0; i < n; ++i)
		{
			if (!valid[i]) { continue; }
			r.valid = true;
			if (mins[i] == mins[i] && (!haveMin || mins[i] < r.min)) { r.min = mins[i]; haveMin = true; }
			if (maxs[i] == maxs[i] && (!haveMax || maxs[i] > r.max)) { r.max = maxs[i]; haveMax = true; }
			if (means[i] == means[i])
			{
				weighted += double(means[i]) * double(weights[i]);
				plain += double(means[i]);
				totalWeight += double(weights[i]);
				++meanCount;
			}
		}
		if (meanCount > 0) { r.mean = totalWeight > 0.0 ? float(weighted / totalWeight) : float(plain / double(meanCount)); }
		return r;
	}

	Json::Value BuildStridedAggregateBody(const CellHistoryRecorder& rec, const std::vector<AggLayer>& layers, uint32_t from, uint32_t windows, uint32_t stride, uint32_t rangeEnd, bool fill, const std::vector<bool>* groupWanted)
	{
		const AggregateStore& agg = rec.Aggregates();
		const uint32_t groups = agg.GroupCount();
		const uint32_t n = windows;
		auto winStart = [&](uint32_t w) { return from + w * stride; };
		auto winEnd = [&](uint32_t w) { const uint64_t e = uint64_t(from) + uint64_t(w + 1) * stride; return uint32_t(e < rangeEnd ? e : rangeEnd); };
		Json::Value r(Json::objectValue);
		Json::Value samples(Json::arrayValue);
		for (uint32_t i = 0; i < n; ++i)
		{
			const SampleHeader& h = rec.Header(winEnd(i) - 1);
			Json::Value s(Json::objectValue);
			s["tick"] = Json::UInt64(h.mTick); s["label"] = h.mLabel.AsCStr(); s["band"] = unsigned(h.mBand);
			samples.append(s);
		}
		r["samples"] = samples;
		Json::Value sizes(Json::objectValue);
		std::vector<uint16_t> u16;
		std::vector<float> f0, f1, f2;
		std::vector<float> sMin, sMean, sMax;
		std::vector<uint16_t> sW;
		for (uint32_t g = 0; g < groups; ++g)
		{
			if (groupWanted && !(*groupWanted)[g]) { continue; }
			const StringCRC gid = agg.GroupAt(g);
			if (!fill) { sizes[gid.AsChar()] = std::string(EncodedLength(n * 2u), 'A'); continue; }
			u16.assign(n, 0);
			for (uint32_t i = 0; i < n; ++i) { uint16_t v = 0; if (agg.GroupSize(winEnd(i) - 1, gid, v)) { u16[i] = v; } }
			sizes[gid.AsChar()] = B64U16(u16.data(), n);
		}
		r["groupSizes"] = sizes;
		Json::Value rows(Json::objectValue);
		for (const AggLayer& l : layers)
		{
			Json::Value lr(Json::objectValue);
			lr["kind"] = l.isFloat ? "float" : "category";
			lr["first"] = l.first;
			if (!l.isFloat) { lr["values"] = l.values; }
			Json::Value gs(Json::objectValue);
			for (uint32_t g = 0; g < groups; ++g)
			{
				if (groupWanted && !(*groupWanted)[g]) { continue; }
				const StringCRC gid = agg.GroupAt(g);
				if (l.isFloat)
				{
					Json::Value gv(Json::objectValue);
					if (!fill)
					{
						const std::string ph(EncodedLength(n * 4u), 'A');
						gv["min"] = ph; gv["mean"] = ph; gv["max"] = ph;
					}
					else
					{
						f0.assign(n, 0.0f); f1.assign(n, 0.0f); f2.assign(n, 0.0f);
						for (uint32_t i = 0; i < n; ++i)
						{
							const uint32_t a = winStart(i), len = winEnd(i) - a;
							sMin.assign(len, 0.0f); sMean.assign(len, 0.0f); sMax.assign(len, 0.0f);
							sW.assign(len, 0);
							std::unique_ptr<bool[]> ok(new bool[len]);
							for (uint32_t j = 0; j < len; ++j)
							{
								AggregateStats st;
								ok[j] = false;
								if (a + j >= l.first && rec.Aggregate(a + j, l.id, gid, st))
								{
									ok[j] = true; sMin[j] = st.mMin; sMean[j] = st.mMean; sMax[j] = st.mMax;
									uint16_t sz = 0; if (agg.GroupSize(a + j, gid, sz)) { sW[j] = sz; }
								}
							}
							const WindowStats ws = ReduceFloatWindow(sMin.data(), sMean.data(), sMax.data(), sW.data(), ok.get(), len);
							f0[i] = ws.min; f1[i] = ws.mean; f2[i] = ws.max;
						}
						gv["min"] = B64F32(f0.data(), n); gv["mean"] = B64F32(f1.data(), n); gv["max"] = B64F32(f2.data(), n);
					}
					gs[gid.AsChar()] = gv;
				}
				else if (!fill) { gs[gid.AsChar()] = std::string(EncodedLength(n * l.enumCount * 2u), 'A'); }
				else
				{
					u16.assign(size_t(n) * l.enumCount, 0);
					for (uint32_t i = 0; i < n; ++i)
					{
						const uint32_t last = winEnd(i) - 1;
						if (last >= l.first) { rec.CategoryCounts(last, l.id, gid, u16.data() + size_t(i) * l.enumCount, l.enumCount); }
					}
					gs[gid.AsChar()] = B64U16(u16.data(), uint32_t(u16.size()));
				}
			}
			lr["groups"] = gs;
			rows[l.id.AsChar()] = lr;
		}
		r["rows"] = rows;
		return r;
	}

	void EventContext::Collect(const CellHistoryRecorder& rec)
	{
		groups.clear();
		types.clear();
		const AggregateStore& agg = rec.Aggregates();
		for (uint32_t g = 1; g < agg.GroupCount(); ++g)
		{
			StringCRC layer;
			uint32_t value = 0;
			if (rec.GroupSource(g, layer, value)) { groups.push_back({ agg.GroupAt(g).AsChar(), layer.Value(), value }); }
		}
		if (const Dia::ValueSchema::ValueSchemaService* schema = rec.Schema())
		{
			for (unsigned int i = 0; i < schema->Count(); ++i)
			{
				const Dia::ValueSchema::ValueInfo* v = schema->At(i);
				if (v != nullptr && v->mKind == Dia::ValueSchema::ValueKind::Event) { types.push_back({ StringCRC(v->mKey.AsCStr()).Value(), v->mKey.AsCStr() }); }
			}
		}
	}

	Json::Value EventToJson(const EventView& e, const EventContext& ctx)
	{
		Json::Value j(Json::objectValue);
		j["sample"] = e.mSample;
		const unsigned int typeCrc = e.mType.Value();
		const char* typeText = nullptr;
		for (const TypeInfo& t : ctx.types) { if (t.crc == typeCrc) { typeText = t.key.c_str(); break; } }
		char fallback[24];
		if (typeText == nullptr) { snprintf(fallback, sizeof(fallback), "crc:%08x", typeCrc); typeText = fallback; }
		j["type"] = typeText;
		const char* groupId = nullptr;
		if (!e.GroupIsNone())
		{
			for (const GroupInfo& g : ctx.groups) { if (g.layerCrc == e.mGroupLayer.Value() && g.value == e.mGroupValue) { groupId = g.id.c_str(); break; } }
		}
		if (groupId != nullptr) { j["group"] = groupId; } else { j["group"] = Json::Value(Json::nullValue); }
		if (e.mCell == HistoryEvent::kNoCell) { j["cell"] = Json::Value(Json::nullValue); } else { j["cell"] = e.mCell; }
		Json::Value payload(Json::nullValue);
		if (e.mPayloadJson != nullptr && e.mPayloadJson[0] != '\0')
		{
			Json::Value parsed;
			Json::Reader reader;
			if (reader.parse(e.mPayloadJson, parsed, false) && parsed.isObject()) { payload = parsed; }
		}
		j["payload"] = payload;
		if (e.mText != nullptr && e.mText[0] != '\0') { j["text"] = e.mText; }
		return j;
	}
}

#endif // DIA_DEBUG
