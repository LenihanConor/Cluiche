#ifdef DIA_DEBUG
#include <DiaCellLayers/CellLayerViewerDomain.h>

#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/QueryResolver.h>
#include <DiaCellLayers/SelectionTools.h>

#include <string.h>
#include <string>
#include <vector>

#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/ValueInfo.h>
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
		const char* const kMetricSelectionSize = "dia.celllayers.viewer.selection_size";
		const char* const kMetricOutlineEdges = "dia.celllayers.viewer.outline_edges";
		const char* const kMetricSelectCommands = "dia.celllayers.viewer.select_commands";
		const char* const kMetricSelectRejected = "dia.celllayers.viewer.select_rejected";
		const char* const kMetricQueryRun = "dia.celllayers.query.run";
		const char* const kMetricQueryRejected = "dia.celllayers.query.rejected";
		const char* const kMetricQueryEvaluateMs = "dia.celllayers.query.evaluate_ms";
		const char* const kMetricQueryLastMatchCount = "dia.celllayers.query.last_match_count";

		const StringCRC kCmdBase("base");
		const StringCRC kCmdGrid("grid");
		const StringCRC kCmdToggle("toggle");
		const StringCRC kCmdSelectTool("select_tool");
		const StringCRC kCmdSelectGroup("select_group");
		const StringCRC kCmdSelectClear("select_clear");
		const StringCRC kCmdSelectPin("select_pin");
		const StringCRC kCmdSelectUnpin("select_unpin");
		const StringCRC kCmdInspectorExpand("inspector_expand");
		const StringCRC kCmdQueryCheck("query_check");
		const StringCRC kCmdQueryRun("query_run");
		const StringCRC kCmdQueryPart("query_part");
		const StringCRC kCmdSetScale("setScale");
	}

	// Parser + resolver + evaluator (fixed-capacity, several KB each), allocated once per domain.
	struct CellLayerViewerDomain::QueryState
	{
		QueryParser		parser;
		QueryResolver	resolver;
		QueryEvaluator	evaluator;
	};

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
		mMetricSelectionSize = metrics.RegisterGauge(StringCRC(kMetricSelectionSize));
		mMetricOutlineEdges = metrics.RegisterGauge(StringCRC(kMetricOutlineEdges));
		mMetricSelectCommands = metrics.RegisterCounter(StringCRC(kMetricSelectCommands));
		mMetricSelectRejected = metrics.RegisterCounter(StringCRC(kMetricSelectRejected));
		mMetricQueryRun = metrics.RegisterCounter(StringCRC(kMetricQueryRun));
		mMetricQueryRejected = metrics.RegisterCounter(StringCRC(kMetricQueryRejected));
		mMetricQueryEvaluateMs = metrics.RegisterGauge(StringCRC(kMetricQueryEvaluateMs));
		mMetricQueryLastMatchCount = metrics.RegisterGauge(StringCRC(kMetricQueryLastMatchCount));
		mSeenReads = mUploader.Reads();
		mSeenUploads = mUploader.Uploads();
		mSeenSkipped = mUploader.SkippedFrames();
		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
	}

	CellLayerViewerDomain::~CellLayerViewerDomain()
	{
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
		delete mResolver;
		delete mQuery;
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
			if (mOverlayCount >= CellLayerRegistry::kMaxOverlays)
			{
				// Drop state for overlays that have since been unregistered.
				uint32_t kept = 0;
				for (uint32_t i = 0; i < mOverlayCount; ++i)
				{
					if (mRegistry.FindOverlay(mOverlays[i].id) != nullptr) { mOverlays[kept++] = mOverlays[i]; }
				}
				mOverlayCount = kept;
				if (mOverlayCount >= CellLayerRegistry::kMaxOverlays) { return false; }
			}
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
		ProcessPointer();

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
		mInspector.SetActive(mRegistry, mActiveGrid, mActiveBase);
		mInspector.Tick(mRegistry, mReader, mSchema, selection);
		for (uint32_t i = 0; i < mRegistry.OverlayCount(); ++i)
		{
			const CellOverlayDesc* o = mRegistry.OverlayAt(i);
			const OverlayState* s = o ? FindOverlayState(o->id) : nullptr;
			if (s != nullptr && s->enabled && !o->draw.empty()) { o->draw(draw, selection); }
		}

		// Update selection outline and metrics
		if (CellSelection* sel = mRegistry.SelectionFor(mActiveGrid))
		{
			mOutline.Update(*sel);
			if (selection != nullptr && selection->Count() > 0 && !mOutline.TooLarge())
			{
				mOutline.Draw(draw);
			}
		}
		mMetricSelectionSize->Set(static_cast<double>(selection != nullptr ? selection->Count() : 0));
		mMetricOutlineEdges->Set(static_cast<double>(mOutline.TooLarge() || selection == nullptr || selection->Count() == 0 ? 0 : mOutline.EdgeCount()));

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
		WriteSelection(out);
		InspectorModel::BuildCardJson(mRegistry, mReader, mSchema, mInspector, mRegistry.SelectionView(mActiveGrid), out["inspector"]);
		Json::Value query(Json::objectValue);
		query["available"] = true;
		if (mQueryCheck.isObject()) { query["check"] = mQueryCheck; }
		if (mQueryLast.isObject()) { query["last"] = mQueryLast; }
		out["query"] = query;
	}

	StringCRC CellLayerViewerDomain::EffectiveGroup() const
	{
		if (mActiveGrid == nullptr) { return StringCRC::kZero; }
		const StringCRC g = mInput.SelectionGroup();
		return SelectionInput::IsGroupOf(mRegistry, mActiveGrid, g) ? g : SelectionInput::DefaultGroup(mRegistry, mSchema, mActiveGrid);
	}

	void CellLayerViewerDomain::WriteSelection(Json::Value& out) const
	{
		const ICellSelectionView* view = mRegistry.SelectionView(mActiveGrid);
		const uint32_t count = view != nullptr ? view->Count() : 0;
		const StringCRC group = EffectiveGroup();
		Json::Value sel(Json::objectValue);
		sel["count"] = count;
		sel["tool"] = mInput.GetTool() == SelectionInput::Tool::Pan ? "pan" : "select";
		sel["group"] = group != StringCRC::kZero ? group.AsChar() : "";
		Json::Value groups(Json::arrayValue);
		if (mActiveGrid != nullptr)
		{
			StringCRC ids[64];
			const uint32_t total = SelectionInput::ListGroups(mRegistry, mSchema, mActiveGrid, ids, 64);
			const uint32_t n = total < 64u ? total : 64u;
			for (uint32_t i = 0; i < n; ++i)
			{
				const CellLayerDesc* d = mRegistry.Find(ids[i]);
				if (d == nullptr) { continue; }
				const char* label = nullptr;
				const char* unit = nullptr;
				LayerText(*d, label, unit);
				Json::Value e(Json::objectValue);
				e["id"] = d->id.AsChar();
				e["label"] = label;
				e["selected"] = (d->id == group);
				groups.append(e);
			}
		}
		sel["groups"] = groups;
		const bool tooLarge = count > 0 && mOutline.TooLarge();
		sel["tooLarge"] = tooLarge;
		if (tooLarge)
		{
			char text[80];
			snprintf(text, sizeof(text), "Selection too large to outline (%u cells)", count);
			sel["tooLargeText"] = text;
		}
		out["selection"] = sel;
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
		else if (cmd == kCmdSelectTool) { ExecuteSetTool(args); }
		else if (cmd == kCmdSelectGroup) { ExecuteSetGroup(args); }
		else if (cmd == kCmdSelectPin) { ExecutePin(args); }
		else if (cmd == kCmdSelectUnpin) { ExecuteUnpin(args); }
		else if (cmd == kCmdInspectorExpand) { ExecuteExpand(args); }
		else if (cmd == kCmdQueryPart) { AddQueryPart(args); }
		else if (cmd == kCmdQueryCheck || cmd == kCmdQueryRun) { RunPanelQuery(cmd == kCmdQueryRun, args); }
		else if (cmd == kCmdSelectClear) { ExecuteSelect(SelectOp::Clear, "cells.select.clear", Json::Value(Json::objectValue)); }
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

	// ---- Sim-hosted commands ----
	namespace
	{
		const char* const kSelectCommands[] = {
			"cells.select.cell", "cells.select.toggle", "cells.select.box", "cells.select.poly",
			"cells.select.category", "cells.select.cells", "cells.select.clear"
		};
		const char* const kQueryCommands[] = { "cells.select.query", "cells.query.check" };
		const char* const kSelectPanelCommands[] = { "cells.select.tool", "cells.select.group", "cells.select.pin", "cells.select.unpin", "cells.inspect", "cells.inspector.expand" };

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

	// Every viewer command is registered through the registrar (sim-hosted: handlers run on SimPU beside the viewer), never with
	// Dia::API. Names, params and results are the former DiaAPI JSON commands'. All are commands, not queries: even cells.query.check
	// and cells.inspect record state for the card. cells.layer.list is NOT registered here: the mirror query of that name
	// (MirrorQueries::LayerList) returns this domain's ListLayers data plus the mirror fields.
	//
	// Params / results (the help text the DiaAPI infos used to carry):
	//   cells.select.cell/.toggle {q,r}; .box {q0,r0,q1,r1}; .poly {points:[[x,y]..]} (>= 3, WORLD coords); .category {layer,value};
	//   .cells {cells:[[q,r]..]} (cap 2048; returns requested, accepted, overCap); .clear {}  -> {ok,count}
	//   cells.select.query / cells.query.check {expr} -> {ok,count} | {error:{message,column,length,suggestion?}}
	//   cells.select.tool {tool:"select"|"pan"} -> {ok,tool}; .group {layer} -> {ok,group}; .pin {freeze?} -> {ok,count,frozen,layers?}; .unpin {} -> {ok}
	//   cells.inspect {} | {group} | {layers:[..]} | {layerId} | {all:true} -> stats JSON; cells.inspector.expand {group,expanded} -> {ok,group,expanded}
	//   cells.layer.base_by_id {id} -> {id,grid_index}; cells.overlay.toggle_by_id {id} -> {id,enabled}
	bool CellLayerViewerDomain::RegisterCommands(IMirrorQueryRegistrar& registrar)
	{
		if (mCommandsRegistered) { return true; }

		struct Def { const char* name; MirrorQueryHandler handler; };
		std::vector<Def> defs;

		defs.push_back({ "cells.layer.base_by_id", [this](const Json::Value& params) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			if (!params.isObject() || !params["id"].isString() || params["id"].asString().empty()) { return SelectError("missing or non-string param 'id'"); }
			const std::string id = params["id"].asString();
			if (!SetBase(StringCRC(id.c_str()))) { return SelectError(std::string("unknown layer id '") + id + "'"); }
			out["id"] = id;
			out["grid_index"] = GridIndexOf(ActiveGrid());
			return out;
		} });
		defs.push_back({ "cells.overlay.toggle_by_id", [this](const Json::Value& params) -> Json::Value
		{
			Json::Value out(Json::objectValue);
			if (!params.isObject() || !params["id"].isString() || params["id"].asString().empty()) { return SelectError("missing or non-string param 'id'"); }
			const std::string id = params["id"].asString();
			if (!ToggleOverlay(StringCRC(id.c_str()))) { return SelectError(std::string("unknown overlay id '") + id + "'"); }
			out["id"] = id;
			out["enabled"] = IsOverlayEnabled(StringCRC(id.c_str()));
			return out;
		} });

		const SelectOp ops[] = { SelectOp::Cell, SelectOp::Toggle, SelectOp::Box, SelectOp::Poly, SelectOp::Category, SelectOp::Cells, SelectOp::Clear };
		for (uint32_t i = 0; i < 7; ++i)
		{
			const SelectOp op = ops[i];
			const char* const name = kSelectCommands[i];
			defs.push_back({ name, [this, op, name](const Json::Value& params) -> Json::Value { return ExecuteSelect(op, name, params); } });
		}
		for (uint32_t i = 0; i < 2; ++i)
		{
			const bool apply = (i == 0);
			const char* const name = kQueryCommands[i];
			defs.push_back({ name, [this, apply, name](const Json::Value& params) -> Json::Value { return ExecuteQueryRecorded(apply, name, params); } });
		}
		defs.push_back({ kSelectPanelCommands[0], [this](const Json::Value& params) -> Json::Value { return ExecuteSetTool(params); } });
		defs.push_back({ kSelectPanelCommands[1], [this](const Json::Value& params) -> Json::Value { return ExecuteSetGroup(params); } });
		defs.push_back({ kSelectPanelCommands[2], [this](const Json::Value& params) -> Json::Value { return ExecutePin(params); } });
		defs.push_back({ kSelectPanelCommands[3], [this](const Json::Value& params) -> Json::Value { return ExecuteUnpin(params); } });
		defs.push_back({ kSelectPanelCommands[4], [this](const Json::Value& params) -> Json::Value { return ExecuteInspect(params); } });
		defs.push_back({ kSelectPanelCommands[5], [this](const Json::Value& params) -> Json::Value { return ExecuteExpand(params); } });

		for (size_t i = 0; i < defs.size(); ++i)
		{
			if (registrar.RegisterCommand(defs[i].name, defs[i].handler)) { continue; }
			DIA_LOG_ERROR("CEL", "viewer commands: registering %s was rejected, rolling back", defs[i].name);
			for (size_t k = 0; k < i; ++k) { registrar.Unregister(defs[k].name); }	// only the ones this call registered
			return false;
		}
		mCommandsRegistered = true;
		return true;
	}

	Json::Value CellLayerViewerDomain::FinishSelectCommand(const char* name, const char* error, Json::Value out)
	{
		if (error != nullptr && error[0] != '\0')
		{
			DIA_LOG_INFO("CEL", "%s rejected: %s", name, error);
			mMetricSelectRejected->Inc();
			return SelectError(error);
		}
		mMetricSelectCommands->Inc();
		return out;
	}

	Json::Value CellLayerViewerDomain::ExecuteSetTool(const Json::Value& params)
	{
		Lock l(mMutex);
		Json::Value out(Json::objectValue);
		std::string error;
		if (!params.isObject() || !params["tool"].isString()) { error = "missing or non-string param 'tool'"; }
		else
		{
			const std::string t = params["tool"].asString();
			if (t == "select") { mInput.SetTool(SelectionInput::Tool::Select); }
			else if (t == "pan") { mInput.SetTool(SelectionInput::Tool::Pan); }
			else { error = "unknown tool '" + t + "'"; }
			if (error.empty()) { out["ok"] = true; out["tool"] = t; }
		}
		return FinishSelectCommand("cells.select.tool", error.c_str(), out);
	}

	Json::Value CellLayerViewerDomain::ExecuteSetGroup(const Json::Value& params)
	{
		Lock l(mMutex);
		Json::Value out(Json::objectValue);
		std::string error;
		if (!params.isObject() || !params["layer"].isString() || params["layer"].asString().empty()) { error = "missing or non-string param 'layer'"; }
		else
		{
			const std::string layer = params["layer"].asString();
			const StringCRC id(layer.c_str());
			if (mActiveGrid == nullptr || !SelectionInput::IsGroupOf(mRegistry, mActiveGrid, id))
			{
				error = "layer '" + layer + "' is not a category layer of the active grid";
			}
			else
			{
				mInput.SetSelectionGroup(id);
				mInputGrid = mActiveGrid;
				out["ok"] = true;
				out["group"] = layer;
			}
		}
		return FinishSelectCommand("cells.select.group", error.c_str(), out);
	}

	Json::Value CellLayerViewerDomain::ExecutePin(const Json::Value& params)
	{
		Lock l(mMutex);
		Json::Value out(Json::objectValue);
		std::string error;
		bool freeze = false;
		const ICellSelectionView* selection = mActiveGrid != nullptr ? mRegistry.SelectionView(mActiveGrid) : nullptr;
		if (params.isObject() && params.isMember("freeze"))
		{
			if (!params["freeze"].isBool()) { error = "non-boolean param 'freeze'"; }
			else { freeze = params["freeze"].asBool(); }
		}
		if (!error.empty()) {}
		else if (mActiveGrid == nullptr || selection == nullptr) { error = "no active grid"; }
		else if (selection->Count() == 0) { error = "nothing selected to pin"; }
		else
		{
			mInspector.SetActive(mRegistry, mActiveGrid, mActiveBase);	// A belongs to the grid the selection is on
			const bool ok = freeze ? mInspector.PinFreeze(mRegistry, mReader, mSchema, mActiveGrid, selection)
				: mInspector.Pin(mRegistry, mActiveGrid, selection);
			if (!ok) { error = "nothing selected to pin"; }
		}
		if (error.empty())
		{
			out["ok"] = true;
			out["count"] = mInspector.PinnedCount();
			out["frozen"] = mInspector.IsFrozen();
			if (mInspector.IsFrozen())
			{
				out["layers"] = mInspector.FrozenLayerCount();
				DIA_LOG_INFO("CEL", "cells.select.pin: pinned %u cells as A, froze %u layers", mInspector.PinnedCount(), mInspector.FrozenLayerCount());
			}
			else { DIA_LOG_INFO("CEL", "cells.select.pin: pinned %u cells as A (live)", mInspector.PinnedCount()); }
		}
		return FinishSelectCommand("cells.select.pin", error.c_str(), out);
	}

	Json::Value CellLayerViewerDomain::ExecuteUnpin(const Json::Value&)
	{
		Lock l(mMutex);
		Json::Value out(Json::objectValue);
		if (mInspector.IsPinned())
		{
			mInspector.Unpin();
			DIA_LOG_INFO("CEL", "cells.select.unpin: cleared A");
		}
		out["ok"] = true;
		return FinishSelectCommand("cells.select.unpin", nullptr, out);
	}

	Json::Value CellLayerViewerDomain::ExecuteExpand(const Json::Value& params)
	{
		Lock l(mMutex);
		Json::Value out(Json::objectValue);
		std::string error;
		if (!params.isObject() || !params["group"].isString() || params["group"].asString().empty()) { error = "missing or non-string param 'group'"; }
		else if (!params["expanded"].isBool()) { error = "missing or non-boolean param 'expanded'"; }
		else
		{
			const std::string group = params["group"].asString();
			bool known = false;
			const uint32_t total = mActiveGrid != nullptr ? mRegistry.LayerCountForGrid(mActiveGrid) : 0;
			for (uint32_t i = 0; i < total && !known; ++i)
			{
				const CellLayerDesc* d = mRegistry.LayerOfGrid(mActiveGrid, i);
				known = d != nullptr && group == d->group.AsCStr();
			}
			if (!known) { error = "no layers in group '" + group + "' on the active grid"; }
			else
			{
				mInspector.SetActive(mRegistry, mActiveGrid, mActiveBase);
				const bool expanded = params["expanded"].asBool();
				mInspector.SetGroupExpanded(group.c_str(), expanded);
				out["ok"] = true;
				out["group"] = group;
				out["expanded"] = expanded;
			}
		}
		return FinishSelectCommand("cells.inspector.expand", error.c_str(), out);
	}

	Json::Value CellLayerViewerDomain::ExecuteInspect(const Json::Value& params)
	{
		Lock l(mMutex);
		std::string error;
		InspectRequest request;
		if (!params.isNull() && !params.isObject()) { error = "params must be an object"; }
		else
		{
			uint32_t forms = 0;
			if (params.isMember("all"))
			{
				if (!params["all"].isBool()) { error = "non-boolean param 'all'"; }
				else if (params["all"].asBool()) { ++forms; request.mode = InspectRequest::Mode::All; }
			}
			if (error.empty() && params.isMember("group"))
			{
				if (!params["group"].isString() || params["group"].asString().empty()) { error = "param 'group' must be a non-empty string"; }
				else
				{
					++forms;
					request.mode = InspectRequest::Mode::Group;
					Dia::ValueSchema::SetText(request.group, params["group"].asString().c_str());
				}
			}
			if (error.empty() && params.isMember("layerId"))
			{
				if (!params["layerId"].isString() || params["layerId"].asString().empty()) { error = "param 'layerId' must be a non-empty string"; }
				else
				{
					++forms;
					request.mode = InspectRequest::Mode::Layers;
					request.AddLayer(StringCRC(params["layerId"].asString().c_str()));
				}
			}
			if (error.empty() && params.isMember("layers"))
			{
				const Json::Value& list = params["layers"];
				if (!list.isArray() || list.empty()) { error = "param 'layers' must be a non-empty array of layer ids"; }
				else
				{
					++forms;
					request.mode = InspectRequest::Mode::Layers;
					for (const Json::Value& v : list)
					{
						if (!v.isString() || v.asString().empty()) { error = "param 'layers' must hold non-empty strings"; break; }
						if (!request.AddLayer(StringCRC(v.asString().c_str()))) { error = "too many layers"; break; }
					}
				}
			}
			if (error.empty() && forms > 1) { error = "conflicting params: use only one of 'group', 'layers' / 'layerId', 'all'"; }
		}

		Json::Value out(Json::objectValue);
		if (error.empty())
		{
			if (mActiveGrid == nullptr) { error = "no active grid"; }
			else if (mInspector.ActiveGrid() != mActiveGrid) { error = "inspector not yet synced to the active grid (no Update since the grid changed)"; }
			else if (!InspectorModel::BuildStatsJson(mRegistry, mReader, mSchema, mInspector, mRegistry.SelectionView(mActiveGrid), request, out))
			{
				error = out["error"].asString();
			}
		}
		return FinishSelectCommand("cells.inspect", error.c_str(), out);
	}

	Json::Value CellLayerViewerDomain::ExecuteSelect(SelectOp op, const char* name, const Json::Value& params)
	{
		Lock l(mMutex);
		return ExecuteSelectLocked(op, name, params);
	}

	Json::Value CellLayerViewerDomain::ExecuteSelectLocked(SelectOp op, const char* name, const Json::Value& params)
	{
		namespace Tools = SelectionTools;

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
			mMetricSelectRejected->Inc();
			return SelectError(error);
		}
		mMetricSelectCommands->Inc();
		return out;
	}

	namespace
	{
		Json::Value QueryErrorJson(const char* message, uint32_t column, uint32_t length, const char* suggestion)
		{
			Json::Value err(Json::objectValue);
			err["message"] = message;
			err["column"] = column;
			err["length"] = length;
			if (suggestion != nullptr && suggestion[0] != '\0') { err["suggestion"] = suggestion; }
			Json::Value out(Json::objectValue);
			out["ok"] = false;
			out["error"] = err;
			return out;
		}
	}

	Json::Value CellLayerViewerDomain::ExecuteQuery(bool apply, const char* name, const Json::Value& params)
	{
		Lock l(mMutex);
		std::string text;
		if (params.isObject() && params["expr"].isString()) { text = params["expr"].asString(); }
		else if (params.isString()) { text = params.asString(); }
		const size_t first = text.find_first_not_of(" \t\r\n");
		// check runs on every keystroke: it stays silent; only select logs (run and reject at Info).
		auto reject = [&](const char* msg, uint32_t col, uint32_t len, const char* sugg) -> Json::Value
		{
			if (apply)
			{
				DIA_LOG_INFO("CEL", "%s rejected: %s", name, msg);
				if (mMetricQueryRejected) { mMetricQueryRejected->Inc(); }
			}
			return QueryErrorJson(msg, col, len, sugg);
		};
		if (first == std::string::npos) { return reject("empty query", 0, 0, nullptr); }
		CellSelection* sel = mRegistry.SelectionFor(mActiveGrid);
		if (sel == nullptr) { return reject("no active grid", 0, 0, nullptr); }

		if (mQuery == nullptr) { mQuery = new QueryState(); }
		const char* const src = text.c_str();
		if (!mQuery->parser.Parse(src))
		{
			const QueryError& e = mQuery->parser.Error();
			return reject(e.mMessage, e.mColumn, e.mLength, e.mSuggestion);
		}
		if (!mQuery->resolver.Resolve(src, mQuery->parser, mRegistry, mActiveGrid, mSchema))
		{
			const QueryError& e = mQuery->resolver.Error();
			return reject(e.mMessage, e.mColumn, e.mLength, e.mSuggestion);
		}
		const uint32_t cellCount = static_cast<uint32_t>(mActiveGrid->width) * mActiveGrid->height;
		const auto t0 = std::chrono::steady_clock::now();
		if (!mQuery->evaluator.Evaluate(mQuery->parser, mQuery->resolver, mReader, cellCount))
		{
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			if (mMetricQueryEvaluateMs) { mMetricQueryEvaluateMs->Set(ms); }
			const QueryError& e = mQuery->evaluator.Error();
			return reject(e.mMessage, e.mColumn, e.mLength, e.mSuggestion);
		}
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		if (mMetricQueryEvaluateMs) { mMetricQueryEvaluateMs->Set(ms); }
		const uint32_t count = mQuery->evaluator.Count();
		if (mMetricQueryLastMatchCount) { mMetricQueryLastMatchCount->Set(static_cast<double>(count)); }
		if (apply)
		{
			if (!mQuery->evaluator.ApplyTo(*sel)) { return reject("query result does not fit the selection", 0, 0, nullptr); }
			DIA_LOG_INFO("CEL", "query run: '%s' -> %u matches", src, count);
			if (mMetricQueryRun) { mMetricQueryRun->Inc(); }
		}
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["count"] = count;
		return out;
	}

	void CellLayerViewerDomain::AddQueryPart(const Json::Value& args)
	{
		if (!args.isObject() || !args["i"].isUInt() || !args["s"].isString())
		{
			DIA_LOG_WARNING("CEL", "query_part ignored: expected {i, s}");
			return;
		}
		const uint32_t index = args["i"].asUInt();
		const std::string& s = args["s"].asString();
		Lock l(mMutex);
		if (index == 0)
		{
			mQueryParts[0] = '\0';
			mQueryPartLength = 0;
			mQueryPartCount = 0;
		}
		if (index != mQueryPartCount)
		{
			DIA_LOG_WARNING("CEL", "query_part ignored: index %u, expected %u", index, mQueryPartCount);
			return;
		}
		if (s.size() > kQueryPartCapacity - mQueryPartLength)
		{
			DIA_LOG_WARNING("CEL", "query_part ignored: query exceeds %u characters", kQueryPartCapacity);
			return;
		}
		memcpy(mQueryParts + mQueryPartLength, s.c_str(), s.size());
		mQueryPartLength += static_cast<uint32_t>(s.size());
		mQueryParts[mQueryPartLength] = '\0';
		++mQueryPartCount;
	}

	void CellLayerViewerDomain::RunPanelQuery(bool run, const Json::Value& args)
	{
		const char* const name = run ? "cells.select.query" : "cells.query.check";
		Json::Value p(Json::objectValue);
		if (args.isObject() && args["e"].isString()) { p["expr"] = args["e"]; }
		else if (args.isObject() && args["expr"].isString()) { p["expr"] = args["expr"]; }
		else if (args.isObject() && args["n"].isUInt())
		{
			bool complete = false;
			{
				Lock l(mMutex);
				complete = (args["n"].asUInt() == mQueryPartCount) && mQueryPartCount > 0;
				p["expr"] = std::string(mQueryParts, mQueryPartLength);
			}
			if (!complete)
			{
				DIA_LOG_WARNING("CEL", "%s: incomplete query (panel transport)", name);
				Json::Value rec(Json::objectValue);
				rec["ok"] = false;
				rec["error"] = QueryErrorJson("incomplete query (panel transport)", 0, 0, nullptr)["error"];
				rec["expr"] = p["expr"];
				Lock l(mMutex);
				(run ? mQueryLast : mQueryCheck) = rec;
				return;
			}
		}
		ExecuteQueryRecorded(run, name, p);
	}

	Json::Value CellLayerViewerDomain::ExecuteQueryRecorded(bool apply, const char* name, const Json::Value& params)
	{
		Json::Value out = ExecuteQuery(apply, name, params);
		std::string text;
		if (params.isObject() && params["expr"].isString()) { text = params["expr"].asString(); }
		else if (params.isString()) { text = params.asString(); }
		Json::Value rec = out;
		rec["expr"] = text;
		Lock l(mMutex);
		(apply ? mQueryLast : mQueryCheck) = rec;
		return out;
	}

	// Routes each gesture command through ExecuteSelectLocked, so metrics and CEL logging match the JSON commands.
	class CellLayerViewerDomain::InputSink : public ISelectionCommands
	{
	public:
		explicit InputSink(CellLayerViewerDomain& d) : mDomain(d) {}
		bool SelectCell(int32_t q, int32_t r) override { return Run(SelectOp::Cell, "cells.select.cell", QR(q, r)); }
		bool ToggleCell(int32_t q, int32_t r) override { return Run(SelectOp::Toggle, "cells.select.toggle", QR(q, r)); }
		bool SelectBox(int32_t q0, int32_t r0, int32_t q1, int32_t r1) override
		{
			Json::Value p(Json::objectValue);
			p["q0"] = q0; p["r0"] = r0; p["q1"] = q1; p["r1"] = r1;
			return Run(SelectOp::Box, "cells.select.box", p);
		}
		bool SelectPolygon(const SelectionTools::PolyPoint* pts, uint32_t n) override
		{
			Json::Value arr(Json::arrayValue);
			for (uint32_t i = 0; i < n; ++i)
			{
				Json::Value pt(Json::arrayValue);
				pt.append(static_cast<double>(pts[i].x));
				pt.append(static_cast<double>(pts[i].y));
				arr.append(pt);
			}
			Json::Value p(Json::objectValue);
			p["points"] = arr;
			return Run(SelectOp::Poly, "cells.select.poly", p);
		}
		bool SelectCategory(StringCRC layerId, float value) override
		{
			Json::Value p(Json::objectValue);
			p["layer"] = layerId.AsChar();
			p["value"] = static_cast<double>(value);
			return Run(SelectOp::Category, "cells.select.category", p);
		}
		bool Clear() override { return Run(SelectOp::Clear, "cells.select.clear", Json::Value(Json::objectValue)); }

	private:
		static Json::Value QR(int32_t q, int32_t r)
		{
			Json::Value p(Json::objectValue);
			p["q"] = q; p["r"] = r;
			return p;
		}
		bool Run(SelectOp op, const char* name, const Json::Value& params)
		{
			const Json::Value out = mDomain.ExecuteSelectLocked(op, name, params);
			return !out.isMember("error");
		}
		CellLayerViewerDomain& mDomain;
	};

	void CellLayerViewerDomain::SubmitPointerFrame(const SelectionInput::PointerFrame& frame, const CellPickerView& view)
	{
		Lock l(mMutex);
		mPointerFrame = frame;
		mPointerFrame.valid = true;
		mPointerView = view;
	}

	void CellLayerViewerDomain::ProcessPointer()
	{
		if (!mPointerFrame.valid) { return; }
		const SelectionInput::PointerFrame frame = mPointerFrame;
		mPointerFrame.valid = false;	// one frame, consumed once
		if (mActiveGrid == nullptr) { return; }
		if (mInputGrid != mActiveGrid || !SelectionInput::IsGroupOf(mRegistry, mActiveGrid, mInput.SelectionGroup()))
		{
			mInput.SetSelectionGroup(SelectionInput::DefaultGroup(mRegistry, mSchema, mActiveGrid));
			mInputGrid = mActiveGrid;
		}
		const StringCRC group = mInput.SelectionGroup();
		const LayerValues values = group != StringCRC::kZero ? mReader.Read(group) : LayerValues();
		InputSink sink(*this);
		mInput.Process(frame, mPointerView, mActiveGrid, sink, values.hasData ? values.values : nullptr, values.hasData ? values.count : 0, group);
	}

	void CellLayerViewerDomain::UnregisterCommands(IMirrorQueryRegistrar& registrar)
	{
		if (!mCommandsRegistered) { return; }
		mCommandsRegistered = false;
		for (const char* n : kSelectCommands) { registrar.Unregister(n); }
		for (const char* n : kSelectPanelCommands) { registrar.Unregister(n); }
		for (const char* n : kQueryCommands) { registrar.Unregister(n); }
		registrar.Unregister("cells.layer.base_by_id");
		registrar.Unregister("cells.overlay.toggle_by_id");
	}
}
#endif // DIA_DEBUG
