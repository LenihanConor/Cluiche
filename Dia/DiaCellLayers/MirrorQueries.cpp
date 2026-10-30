#ifdef DIA_DEBUG
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaCore/Strings/Base64.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

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
		: mRegistry(registry), mViewer(viewer), mSchema(schema), mReader(registry) {}

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

		Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mViewer.StateMutex());
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
		MirrorGridId::Format(MirrorGridId::IndexOf(mRegistry, desc->grid), gridId);
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
		Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mViewer.StateMutex());
		HotkeyTable hotkeys(mRegistry, mSchema);
		Json::Value out(Json::objectValue);
		out["layersVersion"] = mRegistry.LayersVersion();
		Json::Value grids(Json::arrayValue);
		for (uint32_t gi = 0; gi < mRegistry.GridCount(); ++gi)
		{
			const Dia::Graphics::CellGridDesc* g = mRegistry.GridAt(gi);
			hotkeys.Rebuild(g);
			char id[MirrorGridId::kBufferSize];
			MirrorGridId::Format(gi, id);
			Json::Value ge(Json::objectValue);
			ge["id"] = id;
			ge["index"] = gi;
			ge["width"] = g->width;
			ge["height"] = g->height;
			Json::Value layers(Json::arrayValue);
			for (uint32_t i = 0; i < mRegistry.LayerCountForGrid(g); ++i)
			{
				const CellLayerDesc* d = mRegistry.LayerOfGrid(g, i);
				const char hotkey[2] = { hotkeys.HotkeyOf(d->id), 0 };
				Json::Value e(Json::objectValue);
				e["id"] = d->id.AsChar();
				e["schemaKey"] = d->schemaKey.AsChar();
				e["group"] = d->group.AsCStr();
				e["kind"] = d->kind == CellValueKind::Category ? "category" : "float";
				e["hotkey"] = hotkey;
				layers.append(e);
			}
			ge["layers"] = layers;
			grids.append(ge);
		}
		out["grids"] = grids;
		Json::Value overlays(Json::arrayValue);
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			Json::Value e(Json::objectValue);
			e["id"] = o->id.AsChar();
			e["label"] = o->label.AsCStr();
			e["group"] = o->group.AsCStr();
			overlays.append(e);
		}
		out["overlays"] = overlays;
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

		const Dia::Graphics::CellGridDesc* grid = mViewer.ActiveGrid();	// locks internally: read before taking the state lock
		Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mViewer.StateMutex());
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
		Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mViewer.StateMutex());
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

		const Dia::Graphics::CellGridDesc* active = mViewer.ActiveGrid();	// locks internally: read before taking the state lock
		Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mViewer.StateMutex());
		const InspectorState& inspector = mViewer.Inspector();
		if (active == nullptr) { return ErrorObject("no active grid"); }
		if (inspector.ActiveGrid() != active) { return ErrorObject("inspector not yet synced to the active grid"); }
		const ICellSelectionView* sel = mRegistry.SelectionView(active);

		// Cache key: everything the answer depends on besides the frame.
		std::vector<uint64_t> key;
		key.push_back(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sel)));
		key.push_back(sel != nullptr ? sel->Version() : 0);
		key.push_back(sel != nullptr ? sel->Count() : 0);
		key.push_back(static_cast<uint64_t>(request.mode));
		key.push_back(mRegistry.LayersVersion());
		key.push_back((inspector.IsPinned() ? 1u : 0u) | (inspector.IsFrozen() ? 2u : 0u) | (inspector.IsUpdating() ? 4u : 0u));
		key.push_back(inspector.PinnedCount());
		key.push_back(inspector.ShownLayerCount());
		for (uint32_t i = 0; i < inspector.ShownLayerCount(); ++i) { key.push_back(inspector.ShownLayerAt(i).Value()); }
		auto pushLayer = [&](StringCRC id)
		{
			const CellLayerDesc* d = mRegistry.Find(id);
			key.push_back(id.Value());
			key.push_back(d != nullptr && d->version ? d->version() : 0);
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

		if (mStatsCacheValid && mStatsCacheFrame == mFrame && mStatsCacheKey == key) { return mStatsCacheResult; }

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

		mStatsCacheValid = true;
		mStatsCacheFrame = mFrame;
		mStatsCacheKey.swap(key);
		mStatsCacheResult = out;
		return out;
	}

	void RegisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry, const MirrorQueries& queries)
	{
		const MirrorQueries* q = &queries;
		registry.Register(StringCRC("cells.layer.list"), [q](const Json::Value&) { return q->LayerList(); });
		registry.Register(StringCRC("cells.layer.values"), [q](const Json::Value& args) { return q->LayerValues(args); });
		registry.Register(StringCRC("cells.select.get"), [q](const Json::Value& args) { return q->SelectionGet(args); });
		registry.Register(StringCRC("cells.select.stats"), [q](const Json::Value& args) { return q->SelectionStats(args); });
	}

	void UnregisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry)
	{
		registry.Unregister(StringCRC("cells.layer.list"));
		registry.Unregister(StringCRC("cells.layer.values"));
		registry.Unregister(StringCRC("cells.select.get"));
		registry.Unregister(StringCRC("cells.select.stats"));
	}
}
#endif // DIA_DEBUG
