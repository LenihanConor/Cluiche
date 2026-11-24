#ifdef DIA_DEBUG
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaCore/Strings/Base64.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <string.h>
#include <string>
#include <vector>

#include <stdio.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;

	namespace MirrorGridId
	{
		uint32_t IndexOf(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid)
		{
			for (uint32_t i = 0; i < registry.GridCount(); ++i)
			{
				if (registry.GridAt(i) == grid) { return i; }
			}
			return 0;
		}

		void Format(uint32_t index, char (&out)[kBufferSize])
		{
			snprintf(out, sizeof(out), "grid.%u", index);
		}

		void Format(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, char (&out)[kBufferSize])
		{
			if (grid != nullptr && grid->id.AsChar()[0] != 0)
			{
				snprintf(out, sizeof(out), "%s", grid->id.AsChar());
				return;
			}
			Format(IndexOf(registry, grid), out);
		}
	}

	namespace
	{
		Json::Value ErrorObject(const char* message)
		{
			Json::Value e(Json::objectValue);
			e["error"] = message;
			return e;
		}

		// Reads an optional non-negative integer arg. Returns false (error set) when present but not one.
		bool ReadUIntArg(const Json::Value& args, const char* name, uint32_t defaultValue, uint32_t& out, Json::Value& error)
		{
			out = defaultValue;
			if (!args.isObject() || !args.isMember(name)) { return true; }
			const Json::Value& v = args[name];
			bool ok = v.isNumeric();
			double d = 0.0;
			if (ok)
			{
				d = v.asDouble();
				ok = d >= 0.0 && d <= 4294967295.0 && d == static_cast<double>(static_cast<uint64_t>(d));
			}
			if (!ok)
			{
				char msg[96];
				snprintf(msg, sizeof(msg), "param '%s' must be a non-negative integer", name);
				error = ErrorObject(msg);
				return false;
			}
			out = static_cast<uint32_t>(d);
			return true;
		}

		struct CollectState
		{
			uint32_t		skip;
			uint32_t		remaining;
			Json::Value*	indices;
		};

		void CollectFn(uint32_t cellIndex, void* user)
		{
			CollectState& s = *static_cast<CollectState*>(user);
			if (s.skip > 0) { --s.skip; return; }
			if (s.remaining == 0) { return; }
			s.indices->append(cellIndex);
			--s.remaining;
		}
	}

	MirrorQueries::MirrorQueries(const CellLayerRegistry& registry, const CellLayerViewerDomain& viewer, const Dia::ValueSchema::ValueSchemaService* schema)
		: mRegistry(registry), mViewer(viewer), mSchema(schema), mReader(registry)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricQueries[static_cast<int>(Kind::List)] = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.queries.list"));
		mMetricQueries[static_cast<int>(Kind::Values)] = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.queries.values"));
		mMetricQueries[static_cast<int>(Kind::SelectGet)] = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.queries.select_get"));
		mMetricQueries[static_cast<int>(Kind::Stats)] = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.queries.stats"));
		mMetricBytes = metrics.RegisterCounter(StringCRC("dia.celllayers.mirror.bytes_served"));
	}

	void MirrorQueries::RecordServed(Kind kind, const Json::Value& reply) const
	{
		static const char* const kNames[4] = { "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" };
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		const size_t bytes = Json::writeString(builder, reply).size();
		const int k = static_cast<int>(kind);
		if (mMetricQueries[k]) { mMetricQueries[k]->Inc(); }
		if (mMetricBytes) { mMetricBytes->Inc(bytes); }
		DIA_LOG_DEBUG("CEL", "%s served (%u bytes)", kNames[k], static_cast<unsigned>(bytes));
	}

	void MirrorQueries::SetMaxValuesChunk(uint32_t cells)
	{
		mMaxValuesChunk = cells < 1 ? 1 : (cells > kMaxValuesChunk ? kMaxValuesChunk : cells);
	}

	Json::Value MirrorQueries::LayerValues(const Json::Value& args) const
	{
		DIA_PROFILE_SCOPE("celllayers.mirror.layer_values", ::Dia::Observation::Profile::Category::kDiaGraphics);

		if (!args.isObject() || !args.isMember("layer") || !args["layer"].isString())
		{
			return ErrorObject("param 'layer' must be a layer id string");
		}
		const std::string layerName = args["layer"].asString();
		uint32_t offset = 0;
		uint32_t count = 0;
		Json::Value error;
		if (!ReadUIntArg(args, "offset", 0, offset, error)) { return error; }
		const bool countGiven = args.isMember("count");
		if (!ReadUIntArg(args, "count", 0, count, error)) { return error; }
		if (countGiven && count > mMaxValuesChunk)
		{
			DIA_LOG_WARNING("CEL", "cells.layer.values count %u clamped to %u", count, mMaxValuesChunk);	// warns on every clamp
			count = mMaxValuesChunk;
		}

		const StringCRC layerId(layerName.c_str());
		const CellLayerDesc* desc = mRegistry.Find(layerId);
		if (desc == nullptr) { return ErrorObject("unknown layer"); }

		const ::Dia::CellLayers::LayerValues lv = mReader.Read(layerId);
		if (!lv.hasData || lv.values == nullptr) { return ErrorObject("layer has no data in this view"); }
		const uint32_t total = lv.count;
		if (offset > total) { return ErrorObject("param 'offset' is beyond the layer size"); }

		const uint32_t remaining = total - offset;
		if (!countGiven) { count = remaining < mMaxValuesChunk ? remaining : mMaxValuesChunk; }
		if (count > remaining) { count = remaining; }

		std::string data;
		if (count > 0)
		{
			std::vector<uint8_t> bytes(static_cast<size_t>(count) * 4);
			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t bits = 0;
				memcpy(&bits, &lv.values[offset + i], sizeof(bits));
				bytes[i * 4 + 0] = static_cast<uint8_t>(bits);
				bytes[i * 4 + 1] = static_cast<uint8_t>(bits >> 8);
				bytes[i * 4 + 2] = static_cast<uint8_t>(bits >> 16);
				bytes[i * 4 + 3] = static_cast<uint8_t>(bits >> 24);
			}
			Dia::Core::Base64::Encode(bytes.data(), bytes.size(), data);
		}

		char gridId[MirrorGridId::kBufferSize];
		MirrorGridId::Format(mRegistry, desc->grid, gridId);
		Json::Value out(Json::objectValue);
		out["layer"] = layerName;
		out["version"] = static_cast<Json::UInt64>(lv.version);
		out["gridId"] = gridId;
		out["width"] = desc->grid->width;
		out["height"] = desc->grid->height;
		out["offset"] = offset;
		out["count"] = count;
		out["total"] = total;
		out["encoding"] = "f32le-base64";
		out["data"] = data;
		return out;
	}

	Json::Value MirrorQueries::LayerList() const
	{
		// The query shares its name with the DiaAPI command (the DebugServer answers queries first, so it shadows the command):
		// start from the command's data (activeBase, activeGridIndex, label, unit, selected, enabled) and add the mirror fields.
		Json::Value out(Json::objectValue);
		mViewer.ListLayers(out);

		HotkeyTable hotkeys(mRegistry, mSchema);
		out["layersVersion"] = mRegistry.LayersVersion();
		Json::Value& grids = out["grids"];
		for (uint32_t gi = 0; gi < mRegistry.GridCount() && gi < grids.size(); ++gi)
		{
			const Dia::Graphics::CellGridDesc* g = mRegistry.GridAt(gi);
			hotkeys.Rebuild(g);
			char id[MirrorGridId::kBufferSize];
			MirrorGridId::Format(mRegistry, g, id);
			Json::Value& ge = grids[gi];
			ge["id"] = id;
			Json::Value& layers = ge["layers"];
			for (uint32_t i = 0; i < mRegistry.LayerCountForGrid(g) && i < layers.size(); ++i)
			{
				const CellLayerDesc* d = mRegistry.LayerOfGrid(g, i);
				const char hotkey[2] = { hotkeys.HotkeyOf(d->id), 0 };
				Json::Value& e = layers[i];
				if (e["id"].asString() != d->id.AsChar()) { continue; }	// registry changed between the two reads
				e["schemaKey"] = d->schemaKey.AsChar();
				e["hotkey"] = hotkey;
			}
		}
		return out;
	}

	Json::Value MirrorQueries::SelectionGet(const Json::Value& args) const
	{
		uint32_t offset = 0;
		uint32_t count = kMaxSelectionChunk;
		Json::Value error;
		if (!ReadUIntArg(args, "offset", 0, offset, error)) { return error; }
		if (!ReadUIntArg(args, "count", kMaxSelectionChunk, count, error)) { return error; }
		if (count > kMaxSelectionChunk)
		{
			DIA_LOG_WARNING("CEL", "cells.select.get count %u clamped to %u", count, kMaxSelectionChunk);
			count = kMaxSelectionChunk;
		}

		const Dia::Graphics::CellGridDesc* grid = mViewer.ActiveGrid();
		const ICellSelectionView* sel = mRegistry.SelectionView(grid);

		SelectionFingerprint fp;
		if (sel != nullptr) { fp = SelectionFingerprintFn::Compute(*sel); }
		if (offset > fp.mCount) { return ErrorObject("param 'offset' is beyond the selection size"); }

		Json::Value indices(Json::arrayValue);
		if (sel != nullptr && count > 0)
		{
			CollectState state = { offset, count, &indices };
			sel->ForEach(&CollectFn, &state);
		}
		char hex[SelectionFingerprintFn::kHexLength + 1];
		SelectionFingerprintFn::ToHex(fp.mHash, hex);
		Json::Value out(Json::objectValue);
		out["version"] = fp.mVersion;
		out["hash"] = hex;
		out["total"] = fp.mCount;
		out["offset"] = offset;
		out["indices"] = indices;
		return out;
	}

	void MirrorQueries::NotifyFrame()
	{
		++mFrame;
	}

	Json::Value MirrorQueries::SelectionStats(const Json::Value& args) const
	{
		DIA_PROFILE_SCOPE("celllayers.mirror.select_stats", ::Dia::Observation::Profile::Category::kDiaGraphics);

		// Parse and validate before touching anything.
		InspectRequest request;	// default Mode::Shown
		if (!args.isNull() && !args.isObject()) { return ErrorObject("params must be an object"); }
		if (args.isObject() && args.isMember("layers"))
		{
			const Json::Value& layers = args["layers"];
			if (layers.isString())
			{
				if (layers.asString() != "all") { return ErrorObject("param 'layers' must be an array of layer ids or \"all\""); }
				request.mode = InspectRequest::Mode::All;
			}
			else if (layers.isArray())
			{
				if (layers.empty()) { return ErrorObject("param 'layers' must not be an empty array"); }
				request.mode = InspectRequest::Mode::Layers;
				for (const Json::Value& v : layers)
				{
					if (!v.isString() || v.asString().empty()) { return ErrorObject("param 'layers' must hold non-empty strings"); }
					if (!request.AddLayer(StringCRC(v.asString().c_str()))) { return ErrorObject("too many layers"); }
				}
			}
			else { return ErrorObject("param 'layers' must be an array of layer ids or \"all\""); }
		}

		const Dia::Graphics::CellGridDesc* active = mViewer.ActiveGrid();
		const InspectorState& inspector = mViewer.Inspector();
		if (active == nullptr) { return ErrorObject("no active grid"); }
		if (inspector.ActiveGrid() != active) { return ErrorObject("inspector not yet synced to the active grid"); }
		const ICellSelectionView* sel = mRegistry.SelectionView(active);

		// Cache key: everything the answer depends on besides the frame.
		StatsKey key;
		key.Push(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sel)));
		key.Push(sel != nullptr ? sel->Version() : 0);
		key.Push(sel != nullptr ? sel->Count() : 0);
		key.Push(static_cast<uint64_t>(request.mode));
		key.Push(mRegistry.LayersVersion());
			key.Push(InspectorState::SourceKey(mRegistry));	// view change (epoch / live) invalidates immediately
		key.Push((inspector.IsPinned() ? 1u : 0u) | (inspector.IsFrozen() ? 2u : 0u) | (inspector.IsUpdating() ? 4u : 0u));
		key.Push(inspector.PinnedCount());
		key.Push(inspector.ShownLayerCount());
		for (uint32_t i = 0; i < inspector.ShownLayerCount(); ++i) { key.Push(inspector.ShownLayerAt(i).Value()); }
		auto pushLayer = [&](StringCRC id)
		{
			const CellLayerDesc* d = mRegistry.Find(id);
			key.Push(id.Value());
			key.Push(d != nullptr && d->version ? d->version() : 0);
		};
		if (request.mode == InspectRequest::Mode::Layers)
		{
			for (uint32_t i = 0; i < request.layerCount; ++i) { pushLayer(request.layers[i]); }
		}
		else
		{
			for (uint32_t i = 0; i < mRegistry.LayerCountForGrid(active); ++i)	// Shown: shown layers are a subset of these
			{
				const CellLayerDesc* d = mRegistry.LayerOfGrid(active, i);
				if (d != nullptr) { pushLayer(d->id); }
			}
		}

		for (const StatsCacheEntry& e : mStatsCache)
		{
			if (e.valid && e.frame == mFrame && e.key == key) { return e.result; }
		}

		// BuildStatsJson takes a mutable registry because the single-cell block runs detail providers; the stats paths here read only.
		Json::Value out;
		CellLayerRegistry& registry = const_cast<CellLayerRegistry&>(mRegistry);
		if (!InspectorModel::BuildStatsJson(registry, mReader, mSchema, inspector, sel, request, out))
		{
			return ErrorObject(out["error"].isString() ? out["error"].asCString() : "stats failed");	// nothing computed; cache untouched
		}
		++mStatsComputeCount;

		SelectionFingerprint fp;
		if (sel != nullptr) { fp = SelectionFingerprintFn::Compute(*sel); }
		char hex[SelectionFingerprintFn::kHexLength + 1];
		SelectionFingerprintFn::ToHex(fp.mHash, hex);
		out["count"] = fp.mCount;
		out["version"] = fp.mVersion;
		out["hash"] = hex;

		StatsCacheEntry& slot = mStatsCache[mStatsCacheNext];
		mStatsCacheNext = (mStatsCacheNext + 1) % kStatsCacheCapacity;
		slot.valid = true;
		slot.frame = mFrame;
		slot.key = key;
		slot.result = out;
		return out;
	}

	namespace
	{
		const char* const kQueryNames[4] = { "cells.layer.list", "cells.layer.values", "cells.select.get", "cells.select.stats" };

		using Dia::API::ValueParam;
		const ValueParam kValuesParams[] = {
			{ "layer", "string", true, "Id of the layer to read" },
			{ "offset", "int", false, "First cell index (default 0)", nullptr, true, 0.0f, 4.0e9f },
			{ "count", "int", false, "Number of cells (default min(total-offset, chunk); clamped to the chunk size)", nullptr, true, 0.0f, 4.0e9f } };
		const ValueParam kGetParams[] = {
			{ "offset", "int", false, "First selection index (default 0)", nullptr, true, 0.0f, 4.0e9f },
			{ "count", "int", false, "Number of indices (default and maximum 1000)", nullptr, true, 0.0f, 1000.0f } };
		const ValueParam kStatsParams[] = {
			{ "layers", "string", false, "JSON array of layer ids, or \"all\"; omitted = the inspector's shown layers" } };

		Dia::DebugServer::SimHelpInfo QueryHelp(const char* description)
		{
			Dia::DebugServer::SimHelpInfo h;
			h.description = description; h.category = "cells"; h.owner = "DiaCellLayers";
			return h;
		}
	}

	bool RegisterMirrorQueries(IMirrorQueryRegistrar& registrar, const MirrorQueries& queries)
	{
		const MirrorQueries* q = &queries;
		using K = MirrorQueries::Kind;
		MirrorQueryHandler handlers[4] =
		{
			[q](const Json::Value&) { Json::Value r = q->LayerList(); q->RecordServed(K::List, r); return r; },
			[q](const Json::Value& args) { Json::Value r = q->LayerValues(args); q->RecordServed(K::Values, r); return r; },
			[q](const Json::Value& args) { Json::Value r = q->SelectionGet(args); q->RecordServed(K::SelectGet, r); return r; },
			[q](const Json::Value& args) { Json::Value r = q->SelectionStats(args); q->RecordServed(K::Stats, r); return r; },
		};
		Dia::DebugServer::SimHelpInfo help[4] = {
			QueryHelp("List the grids, layers and overlays with their ids, groups, kinds and hotkeys, plus layersVersion."),
			QueryHelp("Read a layer's values for the active grid as little-endian float32, base64 encoded, in chunks."),
			QueryHelp("Read the active grid's selection as ascending cell indices in chunks, with version and hash."),
			QueryHelp("Aggregate per-layer stats over the active grid's selection; cached once per frame.") };
		help[0].noArgs = true;
		help[1].params = kValuesParams; help[1].paramCount = 3;
		help[2].params = kGetParams;    help[2].paramCount = 2;
		help[3].params = kStatsParams;  help[3].paramCount = 1;
		for (int i = 0; i < 4; ++i)
		{
			if (registrar.Register(kQueryNames[i], help[i], handlers[i])) { continue; }
			DIA_LOG_ERROR("CEL", "mirror queries: registering %s was rejected, rolling back", kQueryNames[i]);
			for (int k = 0; k < i; ++k) { registrar.Unregister(kQueryNames[k]); }	// only the ones this call registered
			return false;
		}
		return true;
	}

	void UnregisterMirrorQueries(IMirrorQueryRegistrar& registrar)
	{
		for (const char* name : kQueryNames) { registrar.Unregister(name); }
	}
}
#endif // DIA_DEBUG
