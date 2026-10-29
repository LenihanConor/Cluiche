#ifdef DIA_DEBUG
#include <DiaCellLayers/CellLayerViewerDomain.h>

#include <DiaCellLayers/SelectionTools.h>

#include <string.h>
#include <string>
#include <vector>

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
		, mReader(registry)
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

	const ICellSelectionView* CellLayerViewerDomain::ActiveSelectionView() const
	{
		Lock l(mMutex);
		return mRegistry.SelectionView(mActiveGrid);
	}

	CellSelection* CellLayerViewerDomain::ActiveSelection()
	{
		Lock l(mMutex);
		return mRegistry.SelectionFor(mActiveGrid);
	}

	// Changing the active grid clears the selection of the grid left and of the grid entered.
	void CellLayerViewerDomain::SetActiveGrid(const Dia::Graphics::CellGridDesc* grid)
	{
		if (grid == mActiveGrid) { return; }
		uint32_t dropped = 0;
		const Dia::Graphics::CellGridDesc* const grids[2] = { mActiveGrid, grid };
		for (const Dia::Graphics::CellGridDesc* g : grids)
		{
			if (g == nullptr) { continue; }
			if (CellSelection* sel = mRegistry.SelectionFor(g)) { dropped += sel->Count(); sel->Clear(); }
		}
		if (dropped > 0) { DIA_LOG_INFO("CEL", "Selection cleared: active grid changed (%u cells dropped)", dropped); }
		mActiveGrid = grid;
	}

	void CellLayerViewerDomain::Activate(const CellLayerDesc& layer, const char* why)
	{
		mActiveBase = layer.id;
		SetActiveGrid(layer.grid);
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
					SetActiveGrid(base->grid);
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
		const ICellSelectionView* selection = mRegistry.SelectionView(mActiveGrid);
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			const OverlayState* s = o ? FindOverlayState(o->id) : nullptr;
			if (s != nullptr && s->enabled && !o->draw.empty()) { o->draw(draw, selection); }
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
			const Dia::ValueSchema::ValueInfo* info = mSchema ? mSchema->Find(d->schemaKey) : nullptr;
			const char hk = mHotkeys.HotkeyOf(d->id);
			char hotkey[2] = { hk, 0 };
			Json::Value e(Json::objectValue);
			e["id"] = d->id.AsChar();
			e["label"] = label;
			e["unit"] = unit;
			e["kind"] = d->kind == CellValueKind::Category ? "category" : "float";
			e["hotkey"] = hotkey;
			e["selected"] = (d->id == mActiveBase);
			if (info != nullptr && info->mDescription.Length() > 0) { e["description"] = info->mDescription.AsCStr(); }	// omitted when the schema has none
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
				e["selected"] = (mActiveBase != StringCRC::kZero && d->id == mActiveBase);
				layers.append(e);
			}
			ge["layers"] = layers;
			grids.append(ge);
		}
		out["grids"] = grids;
		out["activeBase"] = mActiveBase != StringCRC::kZero ? mActiveBase.AsChar() : "";
		const uint32_t activeIdx = mActiveBase != StringCRC::kZero ? GridIndexOf(mActiveGrid) : 0xFFFFFFFFu;
		out["activeGridIndex"] = activeIdx == 0xFFFFFFFFu ? -1 : static_cast<int>(activeIdx);
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
		list.description = "List cell layers and overlays grouped by grid; reports activeBase, activeGridIndex and per-layer selected";
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

		RegisterJsonCommands();
		RegisterSelectionCommands();

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

	void CellLayerViewerDomain::RegisterJsonCommands()
	{
		// JSON forms for the E2E/DebugServer channel (the CLI forms take positional args, which JSON params never fill).
		// Convention: a distinct "<name>_by_id" JSON name per CLI command (same-name dual registration logs a warning).
		// Failure = {"error": "..."} (the framework wraps it as success:false); nothing changes on failure.
		Dia::API::CommandInfoJson base;
		base.name = StringCRC("cells.layer.base_by_id");
		base.description = "JSON form of cells.layer.base: params {\"id\": \"<layer id>\"}; returns {id, grid_index}";
		base.category = StringCRC("cells");
		base.owner = "DiaCellLayers";
		base.callback = [this](const Json::Value& params) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			if (!params.isObject() || !params["id"].isString() || params["id"].asString().empty())
			{
				out["error"] = "missing or non-string param 'id'";
				return out;
			}
			const std::string id = params["id"].asString();
			if (!SetBase(StringCRC(id.c_str())))
			{
				out["error"] = std::string("unknown layer id '") + id + "'";
				return out;
			}
			out["id"] = id;
			out["grid_index"] = GridIndexOf(ActiveGrid());
			return out;
		};
		Dia::API::RegisterCommandJson(base);

		Dia::API::CommandInfoJson toggle;
		toggle.name = StringCRC("cells.overlay.toggle_by_id");
		toggle.description = "JSON form of cells.overlay.toggle: params {\"id\": \"<overlay id>\"}; returns {id, enabled}";
		toggle.category = StringCRC("cells");
		toggle.owner = "DiaCellLayers";
		toggle.callback = [this](const Json::Value& params) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			if (!params.isObject() || !params["id"].isString() || params["id"].asString().empty())
			{
				out["error"] = "missing or non-string param 'id'";
				return out;
			}
			const std::string id = params["id"].asString();
			if (!ToggleOverlay(StringCRC(id.c_str())))
			{
				out["error"] = std::string("unknown overlay id '") + id + "'";
				return out;
			}
			out["id"] = id;
			out["enabled"] = IsOverlayEnabled(StringCRC(id.c_str()));
			return out;
		};
		Dia::API::RegisterCommandJson(toggle);
	}

	namespace
	{
		const char* const kSelectCommands[] = {
			"cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly",
			"cells.select.category", "cells.select.cells", "cells.select.clear"
		};

		Json::Value SelectError(const std::string& msg)
		{
			Json::Value out(Json::objectValue);
			out["error"] = msg;
			return out;
		}

		bool GetInt(const Json::Value& params, const char* key, int32_t& out)
		{
			if (!params.isObject() || !params.isMember(key) || !params[key].isInt()) { return false; }	// strings, bools, null, NaN, fractions fail
			out = params[key].asInt();
			return true;
		}

		bool IntPair(const Json::Value& v, int32_t& a, int32_t& b)
		{
			if (!v.isArray() || v.size() != 2 || !v[0u].isInt() || !v[1u].isInt()) { return false; }
			a = v[0u].asInt();
			b = v[1u].asInt();
			return true;
		}
	}

	void CellLayerViewerDomain::RegisterSelectionCommands()
	{
		struct Def { SelectOp op; const char* description; };
		const Def defs[] = {
			{ SelectOp::Cell, "Replace the selection with one cell: params {\"q\": int, \"r\": int}; returns {ok, count}" },
			{ SelectOp::Toggle, "Toggle one cell in the selection: params {\"q\": int, \"r\": int}; returns {ok, count}" },
			{ SelectOp::Box, "Replace the selection with an inclusive cell rectangle (any corner order): params {\"q0\",\"r0\",\"q1\",\"r1\": int}; returns {ok, count}" },
			{ SelectOp::Poly, "Replace the selection with the cells whose centres lie in a polygon: params {\"points\": [[x, y], ...]} (>= 3 points). Points are WORLD coordinates in the cell-centre space (flat-top odd-q, y down), NOT cell coordinates; returns {ok, count}" },
			{ SelectOp::Category, "Replace the selection with every cell of a category layer holding a value: params {\"layer\": \"<layer id>\", \"value\": \"<enum id>\" or number}; returns {ok, count}" },
			{ SelectOp::Cells, "Replace the selection with an explicit cell list: params {\"cells\": [[q, r], ...]} (cap 2048); returns {ok, count, requested, accepted: [[q, r], ...], overCap (only when over the cap)}" },
			{ SelectOp::Clear, "Clear the selection: params {}; returns {ok, count}" },
		};
		for (uint32_t i = 0; i < 7; ++i)
		{
			Dia::API::CommandInfoJson info;
			info.name = StringCRC(kSelectCommands[i]);
			info.description = defs[i].description;
			info.category = StringCRC("cells");
			info.owner = "DiaCellLayers";
			const SelectOp op = defs[i].op;
			const char* const name = kSelectCommands[i];
			info.callback = [this, op, name](const Json::Value& params) -> Json::Value { return ExecuteSelect(op, name, params); };
			Dia::API::RegisterCommandJson(info);
		}
	}

	Json::Value CellLayerViewerDomain::ExecuteSelect(SelectOp op, const char* name, const Json::Value& params)
	{
		namespace Tools = SelectionTools;
		Lock l(mMutex);

		std::string error;
		Json::Value out(Json::objectValue);
		CellSelection* sel = mRegistry.SelectionFor(mActiveGrid);
		if (sel == nullptr) { error = "no active grid"; }
		else
		{
			Tools::ToolResult res;
			int32_t a = 0, b = 0, c = 0, d = 0;
			switch (op)
			{
			case SelectOp::Cell:
			case SelectOp::Toggle:
				if (!GetInt(params, "q", a)) { error = "missing or non-integer param 'q'"; }
				else if (!GetInt(params, "r", b)) { error = "missing or non-integer param 'r'"; }
				else { res = (op == SelectOp::Cell) ? Tools::SelectCell(*sel, a, b) : Tools::ToggleCell(*sel, a, b); }
				break;
			case SelectOp::Box:
				if (!GetInt(params, "q0", a)) { error = "missing or non-integer param 'q0'"; }
				else if (!GetInt(params, "r0", b)) { error = "missing or non-integer param 'r0'"; }
				else if (!GetInt(params, "q1", c)) { error = "missing or non-integer param 'q1'"; }
				else if (!GetInt(params, "r1", d)) { error = "missing or non-integer param 'r1'"; }
				else { res = Tools::SelectBox(*sel, a, b, c, d); }
				break;
			case SelectOp::Poly:
			{
				if (!params.isObject() || !params["points"].isArray()) { error = "missing or non-array param 'points'"; break; }
				const Json::Value& pts = params["points"];
				if (pts.size() < 3) { error = "needs at least 3 points"; break; }
				std::vector<Tools::PolyPoint> verts;
				verts.reserve(pts.size());
				for (Json::ArrayIndex i = 0; i < pts.size(); ++i)
				{
					const Json::Value& p = pts[i];
					if (!p.isArray() || p.size() != 2 || !p[0u].isNumeric() || !p[1u].isNumeric())
					{
						error = "point " + std::to_string(i) + " is not a [x, y] number pair";
						break;
					}
					verts.push_back({ static_cast<float>(p[0u].asDouble()), static_cast<float>(p[1u].asDouble()) });
				}
				if (!error.empty()) { break; }
				res = Tools::SelectPolygon(*sel, verts.data(), static_cast<uint32_t>(verts.size()));
				if (!res.ok) { error = "invalid polygon (non-finite coordinate or grid without cell size)"; }
				break;
			}
			case SelectOp::Category:
			{
				if (!params.isObject() || !params["layer"].isString() || params["layer"].asString().empty()) { error = "missing or non-string param 'layer'"; break; }
				const Json::Value& v = params["value"];
				std::string value;
				if (v.isString()) { value = v.asString(); }
				else if (v.isNumeric())
				{
					char buf[40];
					if (v.isIntegral()) { snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v.asInt64())); }
					else { snprintf(buf, sizeof(buf), "%.9g", v.asDouble()); }
					value = buf;
				}
				else { error = "missing or non-string/number param 'value'"; break; }
				const std::string layer = params["layer"].asString();
				const Tools::CategoryResult cr = Tools::SelectCategory(*sel, mReader, mRegistry, mSchema, StringCRC(layer.c_str()), value.c_str());
				res = cr;
				switch (cr.error)
				{
				case Tools::CategoryError::None: break;
				case Tools::CategoryError::UnknownLayer: error = "unknown layer id '" + layer + "'"; break;
				case Tools::CategoryError::NotCategoryLayer: error = "layer '" + layer + "' is not a category layer"; break;
				case Tools::CategoryError::UnknownValue: error = "unknown value '" + value + "'"; break;
				case Tools::CategoryError::NoData: error = "layer '" + layer + "' has no data in this view"; break;
				case Tools::CategoryError::GridMismatch: error = "layer '" + layer + "' is not on the active grid"; break;
				}
				if (error.empty() && !res.ok) { error = "category selection failed"; }
				break;
			}
			case SelectOp::Cells:
			{
				if (!params.isObject() || !params["cells"].isArray()) { error = "missing or non-array param 'cells'"; break; }
				const Json::Value& cells = params["cells"];
				const uint32_t requested = cells.size();
				// One pair past the cap is enough for SelectCells to report overCap; the rest is validated but not copied.
				std::vector<int32_t> qr;
				qr.reserve(2u * (Tools::kMaxCellsPerCall + 1u));
				for (Json::ArrayIndex i = 0; i < cells.size(); ++i)
				{
					int32_t q = 0, r = 0;
					if (!IntPair(cells[i], q, r)) { error = "cell " + std::to_string(i) + " is not an integer [q, r] pair"; break; }
					if (i <= Tools::kMaxCellsPerCall) { qr.push_back(q); qr.push_back(r); }
				}
				if (!error.empty()) { break; }
				std::vector<uint32_t> accepted(2u * Tools::kMaxCellsPerCall);
				const uint32_t given = requested < Tools::kMaxCellsPerCall + 1u ? requested : Tools::kMaxCellsPerCall + 1u;
				const Tools::CellsResult cr = Tools::SelectCells(*sel, qr.data(), given, accepted.data(), Tools::kMaxCellsPerCall);
				res = cr;
				Json::Value list(Json::arrayValue);
				const uint32_t n = cr.accepted < Tools::kMaxCellsPerCall ? cr.accepted : Tools::kMaxCellsPerCall;
				for (uint32_t i = 0; i < n; ++i)
				{
					Json::Value pr(Json::arrayValue);
					pr.append(static_cast<int>(accepted[2u * i]));
					pr.append(static_cast<int>(accepted[2u * i + 1u]));
					list.append(pr);
				}
				out["requested"] = requested;
				out["accepted"] = list;
				if (cr.overCap)
				{
					out["overCap"] = true;
					DIA_LOG_WARNING("CEL", "%s: %u cells requested, only the first %u are used", name, requested, Tools::kMaxCellsPerCall);
				}
				break;
			}
			case SelectOp::Clear:
				res = Tools::ClearSelection(*sel);
				break;
			}
			if (error.empty())
			{
				out["ok"] = true;
				out["count"] = res.count;
			}
		}

		if (!error.empty())
		{
			DIA_LOG_INFO("CEL", "%s rejected: %s", name, error.c_str());
			return SelectError(error);
		}
		return out;
	}

	void CellLayerViewerDomain::UnregisterCommands()
	{
		if (!mCommandsRegistered) { return; }
		mCommandsRegistered = false;
		for (const char* n : kSelectCommands) { Dia::API::UnregisterCommandJson(StringCRC(n)); }
		Dia::API::UnregisterCommandJson(StringCRC("cells.layer.list"));
		Dia::API::UnregisterCommandJson(StringCRC("cells.layer.base_by_id"));
		Dia::API::UnregisterCommandJson(StringCRC("cells.overlay.toggle_by_id"));
		Dia::API::UnregisterCommand(StringCRC("cells.layer.base"));
		Dia::API::UnregisterCommand(StringCRC("cells.overlay.toggle"));
	}
}
#endif // DIA_DEBUG
