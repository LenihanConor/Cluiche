#ifdef DIA_DEBUG
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/SelectionFingerprint.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaObservation/Log/DiaLog.h>

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
		: mRegistry(registry), mViewer(viewer), mSchema(schema) {}

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

	void RegisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry, const MirrorQueries& queries)
	{
		const MirrorQueries* q = &queries;
		registry.Register(StringCRC("cells.layer.list"), [q](const Json::Value&) { return q->LayerList(); });
		registry.Register(StringCRC("cells.select.get"), [q](const Json::Value& args) { return q->SelectionGet(args); });
	}

	void UnregisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry)
	{
		registry.Unregister(StringCRC("cells.layer.list"));
		registry.Unregister(StringCRC("cells.select.get"));
	}
}
#endif // DIA_DEBUG
