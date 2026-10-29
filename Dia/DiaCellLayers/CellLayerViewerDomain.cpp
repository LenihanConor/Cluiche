#ifdef DIA_DEBUG
#include <DiaCellLayers/CellLayerViewerDomain.h>

#include <string.h>
#include <string>

#include <DiaAPI/DiaAPI.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <chrono>
#include <math.h>
#include <stdio.h>

namespace Dia::CellLayers
{
	using Dia::Core::StringCRC;
	typedef Dia::Core::ScopedLock<Dia::Core::Mutex> Lock;

	namespace
	{
		const char* const kMetricRegistered = "dia.celllayers.viewer.registered_layers";
		const char* const kMetricVisible = "dia.celllayers.viewer.visible_layers";
		const char* const kMetricReadMs = "dia.celllayers.viewer.read_ms";
		const char* const kMetricReads = "dia.celllayers.viewer.reads";
		const char* const kMetricUploads = "dia.celllayers.viewer.uploads";
		const char* const kMetricSkipped = "dia.celllayers.viewer.skipped_frames";

		const StringCRC kCmdBase("base");
		const StringCRC kCmdGrid("grid");
		const StringCRC kCmdToggle("toggle");
		const StringCRC kCmdSetScale("setScale");
	}

	CellLayerViewerDomain::CellLayerViewerDomain(CellLayerRegistry& registry, LayerUploader& uploader, const Dia::ValueSchema::ValueSchemaService* schema, ViewFn view)
		: mRegistry(registry)
		, mUploader(uploader)
		, mSchema(schema)
		, mView(view)
		, mHotkeys(registry, schema)
	{
		if (mSchema != nullptr) { mResolver = new Dia::ValueSchema::RampResolver(*mSchema); }
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricRegistered = metrics.RegisterGauge(StringCRC(kMetricRegistered));
		mMetricVisible = metrics.RegisterGauge(StringCRC(kMetricVisible));
		mMetricReadMs = metrics.RegisterGauge(StringCRC(kMetricReadMs));
		mMetricReads = metrics.RegisterCounter(StringCRC(kMetricReads));
		mMetricUploads = metrics.RegisterCounter(StringCRC(kMetricUploads));
		mMetricSkipped = metrics.RegisterCounter(StringCRC(kMetricSkipped));
		mSeenReads = mUploader.Reads();
		mSeenUploads = mUploader.Uploads();
		mSeenSkipped = mUploader.SkippedFrames();
		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
	}

	CellLayerViewerDomain::~CellLayerViewerDomain()
	{
		UnregisterCommands();
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
		delete mResolver;
	}

	// ---- identity ----
	StringCRC CellLayerViewerDomain::GetDomainId() const { return StringCRC("celllayers"); }
	const char* CellLayerViewerDomain::GetDisplayName() const { return "Cell Layers"; }
	const char* CellLayerViewerDomain::GetDescription() const { return "Per-cell layer viewer: base layer, overlays, legend, hover"; }
	StringCRC CellLayerViewerDomain::GetGroup() const { return StringCRC("Spatial"); }
	Dia::Core::RGBA CellLayerViewerDomain::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kSpatial; }

	// ---- state ----
	StringCRC CellLayerViewerDomain::ActiveBase() const { Lock l(mMutex); return mActiveBase; }
	const Dia::Graphics::CellGridDesc* CellLayerViewerDomain::ActiveGrid() const { Lock l(mMutex); return mActiveGrid; }
	CellLayerViewerDomain::HoverInfo CellLayerViewerDomain::Hover() const { Lock l(mMutex); return mHover; }
	bool CellLayerViewerDomain::IsEmptyState() const { Lock l(mMutex); return mActiveBase == StringCRC::kZero; }

	CellLayerViewerDomain::OverlayState* CellLayerViewerDomain::FindOverlayState(StringCRC id)
	{
		for (uint32_t i = 0; i < mOverlayCount; ++i) { if (mOverlays[i].id == id) { return &mOverlays[i]; } }
		return nullptr;
	}
	const CellLayerViewerDomain::OverlayState* CellLayerViewerDomain::FindOverlayState(StringCRC id) const
	{
		for (uint32_t i = 0; i < mOverlayCount; ++i) { if (mOverlays[i].id == id) { return &mOverlays[i]; } }
		return nullptr;
	}

	bool CellLayerViewerDomain::IsOverlayEnabled(StringCRC id) const
	{
		Lock l(mMutex);
		const OverlayState* s = FindOverlayState(id);
		return s != nullptr && s->enabled;
	}

	uint32_t CellLayerViewerDomain::GridIndexOf(const Dia::Graphics::CellGridDesc* grid) const
	{
		for (uint32_t i = 0; i < mRegistry.GridCount(); ++i) { if (mRegistry.GridAt(i) == grid) { return i; } }
		return 0xFFFFFFFFu;
	}

	void CellLayerViewerDomain::Activate(const CellLayerDesc& layer, const char* why)
	{
		mActiveBase = layer.id;
		mActiveGrid = layer.grid;
		mStarted = true;
		if (mLoggedBase != layer.id)
		{
			DIA_LOG_INFO("CEL", "Viewer base changed to '%s' (%s)", layer.id.AsChar(), why);
			mLoggedBase = layer.id;
		}
	}

	bool CellLayerViewerDomain::SetBase(StringCRC id)
	{
		Lock l(mMutex);
		const CellLayerDesc* layer = mRegistry.Find(id);
		if (layer == nullptr) { return false; }
		Activate(*layer, "command");
		return true;
	}

	bool CellLayerViewerDomain::SetGrid(uint32_t index)
	{
		Lock l(mMutex);
		const Dia::Graphics::CellGridDesc* grid = mRegistry.GridAt(index);
		if (grid == nullptr) { return false; }
		const CellLayerDesc* base = mRegistry.DefaultBase(grid);
		if (base == nullptr) { return false; }
		Activate(*base, "grid selected");
		return true;
	}

	bool CellLayerViewerDomain::ToggleOverlay(StringCRC id)
	{
		Lock l(mMutex);
		if (mRegistry.FindOverlay(id) == nullptr) { return false; }
		OverlayState* s = FindOverlayState(id);
		if (s == nullptr)
		{
			if (mOverlayCount >= CellLayerRegistry::kMaxOverlays) { return false; }
			s = &mOverlays[mOverlayCount++];
			s->id = id;
			s->enabled = false;
		}
		s->enabled = !s->enabled;
		return true;
	}

	// ---- per frame ----
	void CellLayerViewerDomain::ResolveBase()
	{
		if (!mStarted)
		{
			const Dia::Graphics::CellGridDesc* grid = mRegistry.GridAt(0);
			if (grid == nullptr) { return; }
			if (const CellLayerDesc* d = mRegistry.DefaultBase(grid)) { Activate(*d, "initial default"); }
			return;
		}
		if (mActiveBase != StringCRC::kZero)
		{
			if (const CellLayerDesc* base = mRegistry.Find(mActiveBase))
			{
				if (base->grid != mActiveGrid)
				{
					DIA_LOG_WARNING("CEL", "Viewer: layer '%s' is now on a different grid than the active grid; following it", base->id.AsChar());
					mActiveGrid = base->grid;
				}
				return;
			}
		}
		// Active base unregistered (or already in the empty state): fall back to the grid default.
		const CellLayerDesc* fallback = mActiveGrid != nullptr ? mRegistry.DefaultBase(mActiveGrid) : nullptr;
		if (fallback != nullptr) { Activate(*fallback, "fallback to grid default"); return; }
		// The active grid has no layers: adopt the first registered grid that has some (never overrides a grid that still has layers).
		for (uint32_t i = 0; i < mRegistry.GridCount(); ++i)
		{
			const Dia::Graphics::CellGridDesc* grid = mRegistry.GridAt(i);
			if (grid == nullptr || grid == mActiveGrid) { continue; }
			if (const CellLayerDesc* d = mRegistry.DefaultBase(grid))
			{
				DIA_LOG_INFO("CEL", "Viewer: active grid has no layers; adopting grid %u with base '%s'", i, d->id.AsChar());
				Activate(*d, "adopted grid with layers");
				return;
			}
		}
		if (mActiveBase != StringCRC::kZero)
		{
			DIA_LOG_INFO("CEL", "Viewer: no layers left on the active grid; empty state");
			mActiveBase = StringCRC::kZero;
			mLoggedBase = StringCRC::kZero;
		}
	}

	void CellLayerViewerDomain::RefreshLegend(const CellLayerDesc* base)
	{
		if (base == nullptr || mResolver == nullptr)
		{
			mHasLegend = false;
			mLegend = Dia::ValueSchema::LegendData();
			mLegendPrefix[0] = '\0';
			mLegendBase = StringCRC::kZero;
			return;
		}
		if (mHasLegend && mLegendBase == base->id && mLegendReads == mUploader.Reads()) { return; }
		ViewPresentation presentation;
		IViewValueSource* source = mRegistry.ViewValueSource();
		const bool presented = source != nullptr && base->kind == CellValueKind::Float && source->GetPresentation(*base, presentation);
		const PresentedLegend built = mLegendBuilder.BuildPresented(*base, mSchema, *mResolver, true,
			mUploader.ObservedMin(), mUploader.ObservedMax(), presented ? &presentation : nullptr);
		mLegend = built.legend;
		memcpy(mLegendPrefix, built.unitPrefix, sizeof(mLegendPrefix));
		mHasLegend = true;
		mLegendBase = base->id;
		mLegendReads = mUploader.Reads();
	}

	void CellLayerViewerDomain::UpdateHover(Dia::Core::IDebugDraw& draw, const CellLayerDesc* base, UploadResult result)
	{
		mHover = HoverInfo();
		if (base == nullptr || result != UploadResult::Ok || mView.empty() || base->grid == nullptr) { return; }
		const CellPickerView view = mView();
		const Dia::Maths::Vector2D& mouse = draw.GetMousePixel();
		const PickedCell pick = CellPicker::Pick(view, mouse.X(), mouse.Y(), *base->grid);
		if (!pick.valid) { return; }
		uint32_t count = 0;
		const float* values = mUploader.BaseValues(base->id, count);
		if (values == nullptr || pick.index >= count) { return; }
		mHover.valid = true;
		mHover.q = pick.q;
		mHover.r = pick.r;
		mHover.value = values[pick.index];
		mHover.layer = base->id;
	}

	void CellLayerViewerDomain::Update(Dia::Core::IDebugDraw& draw)
	{
		DIA_TRACE_ZONE("celllayers.viewer.update", ::Dia::Observation::Trace::Category::kDiaGraphics);
		Lock l(mMutex);
		ResolveBase();

		const CellLayerDesc* base = mActiveBase != StringCRC::kZero ? mRegistry.Find(mActiveBase) : nullptr;
		UploadResult result;
		{
			DIA_PROFILE_SCOPE("celllayers.viewer.read_upload", ::Dia::Observation::Profile::Category::kDiaGraphics);
			const auto t0 = std::chrono::steady_clock::now();
			result = mUploader.Update(base, mRegistry);
			const std::chrono::duration<double, std::milli> ms = std::chrono::steady_clock::now() - t0;
			mMetricReadMs->Set(ms.count());
		}

		mMetricReads->Inc(mUploader.Reads() - mSeenReads);
		mMetricUploads->Inc(mUploader.Uploads() - mSeenUploads);
		mMetricSkipped->Inc(mUploader.SkippedFrames() - mSeenSkipped);
		mSeenReads = mUploader.Reads();
		mSeenUploads = mUploader.Uploads();
		mSeenSkipped = mUploader.SkippedFrames();
		mMetricRegistered->Set(static_cast<double>(mRegistry.LayerCount()));
		mMetricVisible->Set(static_cast<double>(mActiveGrid != nullptr ? mRegistry.LayerCountForGrid(mActiveGrid) : 0));

		if (result == UploadResult::ReadFailed) { mHealth.SetDegraded(StringCRC("active base read failed")); }
		else if (result != UploadResult::CreatePending) { mHealth.SetOK(); }

		mNoData = (result == UploadResult::NoData);
		RefreshLegend(result == UploadResult::Ok ? base : nullptr);

		if (result == UploadResult::Ok && mUploader.DrawRequest().layer.IsValid())
		{
			draw.RequestDrawCellField(mUploader.DrawRequest());
		}
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			const OverlayState* s = o ? FindOverlayState(o->id) : nullptr;
			if (s != nullptr && s->enabled && !o->draw.empty()) { o->draw(draw, nullptr); }	// selection: null until Feature 2
		}
		UpdateHover(draw, base, result);
	}

	// ---- JSON ----
	void CellLayerViewerDomain::LayerText(const CellLayerDesc& layer, const char*& label, const char*& unit) const
	{
		const Dia::ValueSchema::ValueInfo* info = mSchema ? mSchema->Find(layer.schemaKey) : nullptr;
		label = (info != nullptr && info->mLabel.Length() > 0) ? info->mLabel.AsCStr() : layer.id.AsChar();
		unit = (info != nullptr) ? info->mUnit.AsCStr() : "";
	}

	void CellLayerViewerDomain::WriteLegend(Json::Value& out) const
	{
		Json::Value legend(Json::objectValue);
		legend["available"] = mHasLegend;
		legend["category"] = mHasLegend && mLegend.mIsCategory;
		if (mHasLegend)
		{
			if (mLegendPrefix[0] != '\0')
			{
				legend["unitPrefix"] = mLegendPrefix;
				legend["unit"] = (std::string(mLegendPrefix) + mLegend.mUnit.AsCStr()).c_str();
			}
			else { legend["unit"] = mLegend.mUnit.AsCStr(); }
			legend["min"] = mLegend.mMin;
			legend["max"] = mLegend.mMax;
			legend["ramp"] = mLegend.mRamp.mRamp.mName.AsCStr();
			Json::Value stops(Json::arrayValue);
			for (unsigned i = 0; i < mLegend.mRamp.mRamp.mStopCount; ++i)
			{
				Json::Value st(Json::objectValue);
				st["t"] = mLegend.mRamp.mRamp.mStops[i].mT;
				st["rgba"] = mLegend.mRamp.mRamp.mStops[i].mRgba8;
				stops.append(st);
			}
			legend["stops"] = stops;
			Json::Value ticks(Json::arrayValue);
			for (unsigned i = 0; i < mLegend.mTickCount; ++i) { ticks.append(mLegend.mTicks[i]); }
			legend["ticks"] = ticks;
			if (mLegend.mHasSeen)
			{
				Json::Value seen(Json::objectValue);
				seen["min"] = mLegend.mSeenMin;
				seen["max"] = mLegend.mSeenMax;
				legend["seen"] = seen;
			}
			Json::Value swatches(Json::arrayValue);
			for (unsigned i = 0; i < mLegend.mSwatches.Size(); ++i)
			{
				Json::Value sw(Json::objectValue);
				sw["id"] = mLegend.mSwatches[i].mId.AsCStr();
				sw["label"] = mLegend.mSwatches[i].mLabel.AsCStr();
				sw["rgba"] = mLegend.mSwatches[i].mRgba8;
				swatches.append(sw);
			}
			legend["swatches"] = swatches;
		}
		out["legend"] = legend;
	}

	void CellLayerViewerDomain::WriteHover(Json::Value& out) const
	{
		Json::Value hover(Json::objectValue);
		hover["valid"] = mHover.valid;
		const CellLayerDesc* layer = mHover.valid ? mRegistry.Find(mHover.layer) : nullptr;
		if (mHover.valid && layer != nullptr)
		{
			const Dia::ValueSchema::ValueInfo* info = mSchema ? mSchema->Find(layer->schemaKey) : nullptr;
			const char* label = nullptr;
			const char* unit = nullptr;
			LayerText(*layer, label, unit);
			hover["q"] = mHover.q;
			hover["r"] = mHover.r;
			hover["layer"] = layer->id.AsChar();
			hover["label"] = label;
			hover["value"] = mHover.value;
			hover["unit"] = unit;
			hover["description"] = info != nullptr ? info->mDescription.AsCStr() : "";
			char text[160];
			if (layer->kind == CellValueKind::Category)
			{
				const float rounded = floorf(mHover.value + 0.5f);
				const int idx = (rounded >= 0.0f && rounded < 1.0e6f) ? static_cast<int>(rounded) : -1;	// NaN/negative: no entry
				const char* name = "?";
				if (info != nullptr && idx >= 0 && static_cast<unsigned>(idx) < info->mEnumValues.Size())
				{
					const Dia::ValueSchema::EnumValue& ev = info->mEnumValues[static_cast<unsigned>(idx)];
					name = ev.mLabel.Length() > 0 ? ev.mLabel.AsCStr() : ev.mId.AsCStr();
				}
				snprintf(text, sizeof(text), "%s", name);
			}
			else if (unit[0] != '\0') { snprintf(text, sizeof(text), "%g %s", static_cast<double>(mHover.value), unit); }
			else { snprintf(text, sizeof(text), "%g", static_cast<double>(mHover.value)); }
			hover["text"] = text;
		}
		out["hover"] = hover;
	}

	void CellLayerViewerDomain::GetJSONState(Json::Value& out)
	{
		Lock l(mMutex);
		if (mActiveGrid != mHotkeyGrid || mRegistry.LayerCount() != mHotkeyLayerCount)
		{
			mHotkeys.Rebuild(mActiveGrid);
			mHotkeyGrid = mActiveGrid;
			mHotkeyLayerCount = mRegistry.LayerCount();
		}

		// The panel addresses commands by domain id; its state is keyed by display name, which differs for this domain.
		out["domainId"] = GetDomainId().AsChar();
		out["empty"] = (mActiveBase == StringCRC::kZero);
		if (mNoData) { out["noData"] = true; }	// only present when true: panel contract goldens stay unchanged
		out["activeBase"] = mActiveBase.AsChar();

		Json::Value grids(Json::arrayValue);
		for (uint32_t i = 0; i < mRegistry.GridCount(); ++i)
		{
			const Dia::Graphics::CellGridDesc* g = mRegistry.GridAt(i);
			Json::Value e(Json::objectValue);
			e["index"] = i;
			e["width"] = g->width;
			e["height"] = g->height;
			e["layerCount"] = mRegistry.LayerCountForGrid(g);
			e["active"] = (g == mActiveGrid);
			grids.append(e);
		}
		out["grids"] = grids;
		if (mRegistry.IsMultiGrid()) { out["gridSelector"] = grids; }	// shown only when more than one grid exists

		Json::Value layers(Json::arrayValue);
		Json::Value groups(Json::arrayValue);
		const uint32_t layerCount = mActiveGrid != nullptr ? mRegistry.LayerCountForGrid(mActiveGrid) : 0;
		for (uint32_t i = 0; i < layerCount; ++i)
		{
			const CellLayerDesc* d = mRegistry.LayerOfGrid(mActiveGrid, i);
			if (d == nullptr) { continue; }
			const char* label = nullptr;
			const char* unit = nullptr;
			LayerText(*d, label, unit);
			const char hk = mHotkeys.HotkeyOf(d->id);
			char hotkey[2] = { hk, 0 };
			Json::Value e(Json::objectValue);
			e["id"] = d->id.AsChar();
			e["label"] = label;
			e["unit"] = unit;
			e["kind"] = d->kind == CellValueKind::Category ? "category" : "float";
			e["hotkey"] = hotkey;
			e["selected"] = (d->id == mActiveBase);
			e["group"] = d->group.AsCStr();
			e["groupNumber"] = HotkeyTable::GroupNumber(mRegistry, mActiveGrid, d->id);
			layers.append(e);

			Json::Value* group = nullptr;
			for (Json::ArrayIndex g = 0; g < groups.size(); ++g)
			{
				if (strcmp(groups[g]["name"].asCString(), d->group.AsCStr()) == 0) { group = &groups[g]; break; }
			}
			if (group == nullptr)
			{
				Json::Value ng(Json::objectValue);
				ng["name"] = d->group.AsCStr();
				ng["layers"] = Json::Value(Json::arrayValue);
				groups.append(ng);
				group = &groups[groups.size() - 1];
			}
			(*group)["layers"].append(d->id.AsChar());
		}
		out["layers"] = layers;
		out["groups"] = groups;

		Json::Value overlays(Json::arrayValue);
		Json::Value drawers(Json::arrayValue);
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			const OverlayState* s = FindOverlayState(o->id);
			const bool enabled = s != nullptr && s->enabled;
			Json::Value e(Json::objectValue);
			e["id"] = o->id.AsChar();
			e["label"] = o->label.AsCStr();
			e["group"] = o->group.AsCStr();
			e["enabled"] = enabled;
			overlays.append(e);
			Json::Value d(Json::objectValue);
			d["name"] = o->id.AsChar();
			d["enabled"] = enabled;
			drawers.append(d);
		}
		out["overlays"] = overlays;
		out["drawers"] = drawers;

		Json::Value stats(Json::objectValue);
		stats["reads"] = static_cast<Json::UInt64>(mUploader.Reads());
		stats["uploads"] = static_cast<Json::UInt64>(mUploader.Uploads());
		stats["skippedFrames"] = static_cast<Json::UInt64>(mUploader.SkippedFrames());
		stats["readFailures"] = static_cast<Json::UInt64>(mUploader.ReadFailures());
		out["stats"] = stats;

		WriteLegend(out);
		WriteHover(out);
	}

	void CellLayerViewerDomain::OnCommand(StringCRC cmd, const Json::Value& args)
	{
		if (cmd == kCmdBase)
		{
			if (args.isMember("id") && args["id"].isString()) { SetBase(StringCRC(args["id"].asCString())); }
		}
		else if (cmd == kCmdGrid)
		{
			if (args.isMember("index") && args["index"].isNumeric() && args["index"].isUInt()) { SetGrid(args["index"].asUInt()); }
		}
		else if (cmd == kCmdToggle)
		{
			if (args.isMember("drawer") && args["drawer"].isString()) { ToggleOverlay(StringCRC(args["drawer"].asCString())); }
		}
		// kCmdSetScale and unknown commands: no-op (this domain has no scale)
		(void)kCmdSetScale;
	}

	void CellLayerViewerDomain::ListLayers(Json::Value& out) const
	{
		Lock l(mMutex);
		Json::Value grids(Json::arrayValue);
		for (uint32_t gi = 0; gi < mRegistry.GridCount(); ++gi)
		{
			const Dia::Graphics::CellGridDesc* g = mRegistry.GridAt(gi);
			Json::Value ge(Json::objectValue);
			ge["index"] = gi;
			ge["width"] = g->width;
			ge["height"] = g->height;
			Json::Value layers(Json::arrayValue);
			for (uint32_t i = 0; i < mRegistry.LayerCountForGrid(g); ++i)
			{
				const CellLayerDesc* d = mRegistry.LayerOfGrid(g, i);
				const char* label = nullptr;
				const char* unit = nullptr;
				LayerText(*d, label, unit);
				Json::Value e(Json::objectValue);
				e["id"] = d->id.AsChar();
				e["label"] = label;
				e["unit"] = unit;
				e["group"] = d->group.AsCStr();
				e["kind"] = d->kind == CellValueKind::Category ? "category" : "float";
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
			const OverlayState* s = FindOverlayState(o->id);
			Json::Value e(Json::objectValue);
			e["id"] = o->id.AsChar();
			e["label"] = o->label.AsCStr();
			e["group"] = o->group.AsCStr();
			e["enabled"] = s != nullptr && s->enabled;
			overlays.append(e);
		}
		out["overlays"] = overlays;
	}

	// ---- DiaAPI ----
	void CellLayerViewerDomain::RegisterCommands()
	{
		if (mCommandsRegistered) { return; }
		mCommandsRegistered = true;

		Dia::API::CommandInfoJson list;
		list.name = StringCRC("cells.layer.list");
		list.description = "List cell layers and overlays grouped by grid";
		list.category = StringCRC("cells");
		list.owner = "DiaCellLayers";
		list.noArgs = true;
		list.callback = [this](const Json::Value&) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			ListLayers(out);
			return out;
		};
		Dia::API::RegisterCommandJson(list);

		Dia::API::RegisterCommand({
			StringCRC("cells.layer.base"),
			"Make a cell layer the active base (changes the active grid if needed)",
			StringCRC("cells"), "DiaCellLayers", "1.0.0",
			"cells.layer.base temperature",
			[this](const Dia::API::CommandArgs& args) -> int
			{
				if (args.positionalArgs.Size() < 1) { return 2; }
				return SetBase(StringCRC(args.positionalArgs[0])) ? 0 : 1;
			}
		});

		Dia::API::RegisterCommand({
			StringCRC("cells.overlay.toggle"),
			"Toggle a cell overlay on or off",
			StringCRC("cells"), "DiaCellLayers", "1.0.0",
			"cells.overlay.toggle rivers",
			[this](const Dia::API::CommandArgs& args) -> int
			{
				if (args.positionalArgs.Size() < 1) { return 2; }
				return ToggleOverlay(StringCRC(args.positionalArgs[0])) ? 0 : 1;
			}
		});
	}

	void CellLayerViewerDomain::UnregisterCommands()
	{
		if (!mCommandsRegistered) { return; }
		mCommandsRegistered = false;
		Dia::API::UnregisterCommandJson(StringCRC("cells.layer.list"));
		Dia::API::UnregisterCommand(StringCRC("cells.layer.base"));
		Dia::API::UnregisterCommand(StringCRC("cells.overlay.toggle"));
	}
}
#endif // DIA_DEBUG
