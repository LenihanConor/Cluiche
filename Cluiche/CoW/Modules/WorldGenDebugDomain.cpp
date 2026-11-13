#include "Modules/WorldGenDebugDomain.h"
#ifdef DIA_DEBUG

#include "Modules/WorldGenerationModule.h"
#include <Shared/WorldGeneration/WorldFiles.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

namespace CoW {

namespace {
    using namespace WorldGeneration;

    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kCmdRegenerate("regenerate");
    const Dia::Core::StringCRC kCmdSetSeed("setSeed");
    const Dia::Core::StringCRC kCmdSetSize("setSize");
    const Dia::Core::StringCRC kCmdPresetLoad("presetLoad");
    const Dia::Core::StringCRC kCmdPresetSave("presetSave");
    const Dia::Core::StringCRC kCmdSelectCell("selectCell");
    const Dia::Core::StringCRC kCmdClearCell("clearCell");
    const Dia::Core::StringCRC kCmdSetColourMode("setColourMode");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdExpandGuarantee("expandGuarantee");

    const Dia::Core::StringCRC kCmdValidate("validate");
    const Dia::Core::StringCRC kCmdExport("export");
    const Dia::Core::StringCRC kCmdImport("import");
    const Dia::Core::StringCRC kCmdCancel("cancel");
    const Dia::Core::StringCRC kCmdSelectWorld("selectWorld");
    const Dia::Core::StringCRC kCmdDiscard("discard");

    constexpr unsigned kMaxAttemptRows = 12;       // the report query's page size
    constexpr size_t   kMaxTextChars   = 160;      // diagnostics, warnings
    constexpr size_t   kMaxErrorChars  = 300;

    std::string Clip(const std::string& text, size_t max)
    {
        return text.size() > max ? text.substr(0, max) + "..." : text;
    }

    // Report outcome as the card names it.
    const char* OutcomeText(GenerationOutcome outcome)
    {
        switch (outcome)
        {
        case GenerationOutcome::Success:       return "Complete";
        case GenerationOutcome::Unsatisfiable: return "Unsatisfiable";
        case GenerationOutcome::Timeout:       return "TIMEOUT";
        case GenerationOutcome::Cancelled:     return "CANCEL";
        case GenerationOutcome::Error:         return "FAILURE";
        }
        return "Unknown";
    }

    // The fields of an export / import reply the panel shows; warnings are counted and the first few listed.
    Json::Value CompactFileResult(const Json::Value& r)
    {
        Json::Value out(Json::objectValue);
        if (!r.isObject())
        {
            out["success"] = false;
            out["error"] = "no result";
            return out;
        }
        for (const char* key : { "success", "name", "path", "bytes", "revision", "status", "field", "line", "column" })
        {
            if (r.isMember(key))
            {
                out[key] = r[key];
            }
        }
        if (r.isMember("error"))
        {
            out["error"] = r["error"].isString() ? Json::Value(Clip(r["error"].asString(), kMaxErrorChars)) : r["error"];
        }
        const Json::Value& warnings = r["warnings"];
        out["warningCount"] = warnings.isArray() ? warnings.size() : 0u;
        Json::Value listed(Json::arrayValue);
        for (Json::ArrayIndex i = 0; warnings.isArray() && i < warnings.size() && i < 3; ++i)
        {
            listed.append(Clip(warnings[i].asString(), kMaxTextChars));
        }
        out["warnings"] = listed;
        return out;
    }

    // The "unlock" arg a locked world needs, carried into the session call when the panel sent it.
    void CarryUnlock(const Json::Value& args, Json::Value& set)
    {
        if (args.isObject() && args["unlock"].isBool())
        {
            set["unlock"] = args["unlock"];
        }
    }

    const char* const kDefaultMode = "terrain";
    const char* const kViewDefaultId = "relief";   // the viewer's default base layer, shown to the panel as "terrain"

    int StageIndex(const std::string& name)
    {
        for (int i = 0; i < static_cast<int>(TerrainStage::Count); ++i)
        {
            if (name == TerrainStageName(static_cast<TerrainStage>(i)))
            {
                return i;
            }
        }
        return 0;
    }

    // "landFraction" -> "Land fraction"
    std::string LabelFromKey(const std::string& key)
    {
        std::string out;
        for (size_t i = 0; i < key.size(); ++i)
        {
            const char c = key[i];
            if (i == 0)
            {
                out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            else if (std::isupper(static_cast<unsigned char>(c)))
            {
                out += ' ';
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            else
            {
                out += c;
            }
        }
        return out;
    }

    bool IsKnownMode(const WorldGenSession& session, const std::string& mode)
    {
        if (mode == kDefaultMode)
        {
            return true;
        }
        return session.Generator().GetLayers().FindLayer(mode.c_str()) >= 0;
    }

    void WarnIfFailed(const char* what, const Json::Value& result)
    {
        if (result.isObject() && result.isMember("success") && result["success"].isBool() && !result["success"].asBool())
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: %s failed: %s", what,
                result.isMember("error") && result["error"].isString() ? result["error"].asCString() : "unknown error");
        }
    }
}

WorldGenDebugDomain::WorldGenDebugDomain(WorldGenerationModule& module)
    : mModule(module)
    , mDrawers{ { "Tiles", true, true }, { "Grid lines", false, true }, { "Selection", true, true },
                { "Region edges", false, false }, { "Sites", false, false }, { "Evidence", false, false } }
    , mLastSave(Json::nullValue)
{}

void WorldGenDebugDomain::SetView(IWorldGenView* view)
{
    if (mView != view)
    {
        mView = view;
        mStaticDirty = true;   // view.modes come from the viewer's registered base layers
    }
}

Dia::Core::StringCRC WorldGenDebugDomain::GetDomainId()   const { return Dia::Core::StringCRC("WorldGen"); }
const char*          WorldGenDebugDomain::GetDisplayName() const { return "World Generation"; }
const char*          WorldGenDebugDomain::GetDescription() const { return "S-1 terrain generation: shape sliders, presets, timings and cell inspector"; }
Dia::Core::StringCRC WorldGenDebugDomain::GetGroup()       const { return Dia::Core::StringCRC("Systems"); }
Dia::Core::RGBA      WorldGenDebugDomain::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

void WorldGenDebugDomain::RebuildStatic()
{
    const WorldGenSession& session = mModule.GetSession();
    const Json::Value params = session.Params();

    StaticPart next;

    // controls, grouped by first stage affected
    struct Row { int stage; std::string key; };
    std::vector<Row> rows;
    const Json::Value& ranges = params["ranges"];
    for (const std::string& key : ranges.getMemberNames())
    {
        rows.push_back(Row{ StageIndex(ranges[key]["firstStage"].asString()), key });
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.stage < b.stage; });

    next.controlDefs = Json::Value(Json::objectValue);
    for (const Row& row : rows)
    {
        const Json::Value& range = ranges[row.key];
        const Json::Value& value = params["params"][row.key];
        Json::Value c(Json::objectValue);
        c["key"]       = row.key;
        c["label"]     = LabelFromKey(row.key);
        c["stage"]     = row.stage;
        c["stageName"] = range["firstStage"];
        if (range.isMember("values"))
        {
            c["type"]    = "select";
            c["values"]  = range["values"];
            c["default"] = range["values"][range["default"].asUInt()];
        }
        else
        {
            c["type"]    = "slider";
            c["min"]     = range["min"];
            c["max"]     = range["max"];
            c["step"]    = value.type() == Json::uintValue ? Json::Value(1) : Json::Value(0.01);
            c["default"] = range["default"];
        }
        next.controlKeys.push_back(row.key);
        next.controlDefs[row.key] = c;
    }

    // presets (directory scan: the expensive part, hence cached)
    next.presets = Json::Value(Json::arrayValue);
    const Json::Value presetList = session.Presets();
    for (const Json::Value& p : presetList)
    {
        Json::Value e(Json::objectValue);
        e["id"] = p["id"];
        e["displayName"] = p["displayName"];
        next.presets.append(e);
    }

    // view modes + picker layers
    next.modes = Json::Value(Json::arrayValue);
    next.layers = Json::Value(Json::arrayValue);
    auto addLayer = [&next](const std::string& id, const std::string& label, const std::string& group) {
        Json::Value e(Json::objectValue);
        e["id"] = id;
        e["label"] = label;
        e["group"] = group;
        next.layers.append(e);
    };
    next.modes.append(kDefaultMode);
    if (mView != nullptr)
    {
        std::string reliefGroup = "Terrain";
        Json::Value others(Json::arrayValue);
        mView->ListColourGroups([&](const char* id, const char* group) {
            if (std::strcmp(id, kViewDefaultId) == 0)
            {
                reliefGroup = group;
                return;
            }
            next.modes.append(id);
            Json::Value e(Json::objectValue);
            e["id"] = id;
            e["label"] = LabelFromKey(id);
            e["group"] = group;
            others.append(e);
        });
        addLayer(kDefaultMode, "Relief", reliefGroup);
        for (const Json::Value& e : others)
        {
            next.layers.append(e);
        }
    }
    else
    {
        addLayer(kDefaultMode, "Relief", "Terrain");
        for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
        {
            next.modes.append(TerrainLayers::GetLayerInfo(i).name);
            addLayer(TerrainLayers::GetLayerInfo(i).name, LabelFromKey(TerrainLayers::GetLayerInfo(i).name), "Layers");
        }
    }

    for (int i = 0; i < kImplementedStageCount; ++i)
    {
        next.timingNames.push_back(TerrainStageName(static_cast<TerrainStage>(i)));
    }

    const bool changed = !mStaticBuilt || !(next.controlDefs == mStatic.controlDefs) || next.controlKeys != mStatic.controlKeys ||
                         !(next.presets == mStatic.presets) || !(next.modes == mStatic.modes) || !(next.layers == mStatic.layers) || next.timingNames != mStatic.timingNames;
    if (changed)
    {
        mStatic = std::move(next);
        ++mStaticVersion;
    }
    mStaticBuilt = true;
    mStaticDirty = false;
    mStaticBuiltAt = std::chrono::steady_clock::now();
}

void WorldGenDebugDomain::GetJSONState(Json::Value& out)
{
    const WorldGenSession& session = mModule.GetSession();

    // Refresh the cached static part only when needed (first call, after a presetSave, or every kStaticRefreshSeconds so
    // preset files added on disk show up). Nothing here is O(cells).
    if (!mStaticBuilt || mStaticDirty ||
        std::chrono::steady_clock::now() - mStaticBuiltAt >= std::chrono::seconds(kStaticRefreshSeconds))
    {
        RebuildStatic();
    }

    const bool full = mForceFull || mStaticVersion != mLastFullVersion || ++mFramesSinceFull >= kFullFrameInterval;
    if (full)
    {
        mForceFull = false;
        mLastFullVersion = mStaticVersion;
        mFramesSinceFull = 0;
    }

    const Json::Value status = session.Status();

    // Param values only change when the shape does; Params() formats every float (shortest round-trip text), so it is
    // re-run only then, not every frame.
    if (!mHaveValues || !(session.Shape() == mValuesShape))
    {
        mValues = session.Params()["params"];
        mValuesShape = session.Shape();
        mHaveValues = true;
    }

    out["full"]          = full;
    out["staticVersion"] = mStaticVersion;
    out["drawers"] = Json::Value(Json::arrayValue);

    // stats
    Json::Value& stats = out["stats"] = Json::Value(Json::objectValue);
    stats["status"]         = status["state"];
    stats["revision"]       = status["revision"];
    stats["seed"]           = status["seed"];
    stats["size"]           = status["size"];
    stats["width"]          = status["dimensions"]["width"];
    stats["height"]         = status["dimensions"]["height"];
    stats["locked"]         = status["locked"];
    stats["source"]         = status["source"];
    stats["lastMs"]         = status["lastMs"];
    stats["preset"]         = session.PresetId();
    stats["presetModified"] = session.PresetModified();
    stats["stagesRan"]      = status["stagesRan"];

    // controls: current values; the definitions ride along only in a full frame
    Json::Value& controls = out["controls"] = Json::Value(Json::arrayValue);
    for (const std::string& key : mStatic.controlKeys)
    {
        Json::Value c = full ? mStatic.controlDefs[key] : Json::Value(Json::objectValue);
        c["key"]   = key;
        c["value"] = mValues[key];
        controls.append(c);
    }

    if (full)
    {
        out["presets"] = mStatic.presets;
    }

    // sizes
    Json::Value& sizes = out["sizes"] = Json::Value(Json::arrayValue);
    for (WorldSize s : { WorldSize::Small, WorldSize::Standard, WorldSize::Large })
    {
        Json::Value e(Json::objectValue);
        e["name"] = WorldSizeName(s);
        if (full)
        {
            const GridDimensions d = WorldSizeDimensions(s);
            e["width"] = d.width;
            e["height"] = d.height;
        }
        e["selected"] = status["size"].asString() == WorldSizeName(s);
        sizes.append(e);
    }

    // timings: per-stage ms recorded by the generator on its last Update
    Json::Value& timings = out["timings"] = Json::Value(Json::arrayValue);
    const StageTiming* st = session.Generator().GetTimings();
    for (int i = 0; i < kImplementedStageCount; ++i)
    {
        Json::Value e(Json::objectValue);
        e["stage"] = i;
        if (full)
        {
            e["name"] = mStatic.timingNames[static_cast<size_t>(i)];
        }
        e["ms"] = static_cast<double>(st[i].milliseconds);
        e["ran"] = st[i].ranLastUpdate;
        timings.append(e);
    }

    // view
    Json::Value& view = out["view"] = Json::Value(Json::objectValue);
    bool hasSelection = mHasSelection;
    int selX = mSelX;
    int selY = mSelY;
    Json::Value drawers(Json::arrayValue);
    Json::Value siteKinds(Json::arrayValue);   // stays empty without a hosted view
    if (mView != nullptr)
    {
        const char* mode = mView->GetColourMode();
        view["colourMode"] = std::strcmp(mode, kViewDefaultId) == 0 ? kDefaultMode : mode;
        auto add = [&drawers, this](const char* name, bool enabled, bool available) {
            Json::Value e(Json::objectValue);
            e["name"] = name;
            const std::string label = mView->OverlayLabel(name);
            if (label != name)
                e["label"] = label;   // omitted when it would repeat the name (keeps view.drawers small)
            e["enabled"] = enabled;
            e["available"] = available;
            drawers.append(e);
        };
        add("Tiles", true, true);
        mView->ListOverlays([&add](const char* id, bool enabled, bool available) { add(id, enabled, available); });
        add("Selection", true, true);
        Json::Value legend(Json::nullValue);
        mView->FillLegend(legend);
        view["legend"] = legend;
        // AC11: one row per site kind with its toggle id, colour and shape (the Sites overlay draws them generically).
        mView->ListSiteKinds([&siteKinds](const IWorldGenView::SiteKindRow& k) {
            char colour[8];
            std::snprintf(colour, sizeof(colour), "#%06x", k.info->debugColour & 0xFFFFFFu);
            Json::Value e(Json::objectValue);
            e["overlay"] = k.overlay;
            e["kind"] = k.info->id;
            e["colour"] = colour;
            e["shape"] = WorldGeneration::SiteShapeName(k.info->debugShape);
            e["enabled"] = k.enabled;
            e["count"] = k.count;
            siteKinds.append(e);
        });
        hasSelection = mView->GetSelection(selX, selY);
        const std::string hidden = mView->EvidenceHiddenReason();
        if (!hidden.empty())
            view["evidenceHidden"] = hidden;
        if (mView->ExpandedGuarantee() != 0)
            view["expandedGuarantee"] = static_cast<Json::UInt>(mView->ExpandedGuarantee());
    }
    else
    {
        view["colourMode"] = mColourMode;
        for (const Drawer& d : mDrawers)
        {
            Json::Value e(Json::objectValue);
            e["name"] = d.name;
            e["enabled"] = d.enabled;
            e["available"] = d.available;
            drawers.append(e);
        }
    }
    if (full)
    {
        view["modes"] = mStatic.modes;
        view["layers"] = mStatic.layers;
    }
    view["drawers"] = drawers;
    view["siteKinds"] = siteKinds;

    // cell inspector
    out["cell"] = Json::Value(Json::nullValue);
    if (hasSelection)
    {
        Json::Value args(Json::objectValue);
        args["x"] = selX;
        args["y"] = selY;
        Json::Value cell = session.Cell(args);
        if (cell.isObject() && cell["success"].asBool())
        {
            cell["selected"]["x"] = selX;
            cell["selected"]["y"] = selY;
            out["cell"] = cell;
        }
        else
        {
            out["cellError"] = cell.isMember("error") ? cell["error"] : Json::Value("no cell");
        }
    }

    out["lastSave"] = mLastSave;

    // Panel sections: the card first (it reads the session's report/import state), then the three sections.
    UpdateOutcomeCard();
    out["validation"] = BuildValidation();
    if (!mValidateError.empty())
    {
        out["validateError"] = mValidateError;
    }
    out["file"]     = BuildFile(full);
    out["progress"] = BuildProgress();
}

void WorldGenDebugDomain::RefreshWorlds()
{
    mWorlds = mModule.GetSession().Worlds()["worlds"];
    mWorldsBuilt = true;
    mWorldsDirty = false;
    mWorldsBuiltAt = std::chrono::steady_clock::now();
    bool found = mSelectedWorld.empty();
    for (const Json::Value& w : mWorlds)
    {
        found = found || w["name"].asString() == mSelectedWorld;
    }
    if (!found)
    {
        mSelectedWorld.clear();   // the file went away
    }
}

void WorldGenDebugDomain::UpdateOutcomeCard()
{
    const WorldGenSession& s = mModule.GetSession();
    const GenerationReport* report = s.LastReport();
    const bool newReport = report != nullptr && s.ReportRevision() != mSeenReportRevision;
    const bool importNewer = s.IsImported() && s.Revision() != mCardImportRevision;

    // The import is the newer event when the report predates it: a success report clears the imported flag, and on the first
    // sight of an already-imported session the import is what is on screen.
    if (newReport && !(importNewer && (report->outcome == GenerationOutcome::Success || mCard.isNull())))
    {
        mSeenReportRevision = s.ReportRevision();
        Json::Value card(Json::objectValue);
        card["kind"] = "generation";
        card["outcome"] = OutcomeText(report->outcome);
        card["message"] = std::string(report->failureMessage);
        card["attempts"] = static_cast<Json::UInt>(report->attempts.Size());
        card["maxAttempts"] = static_cast<Json::UInt>(report->maxAttempts);
        card["revision"] = static_cast<Json::UInt>(report->worldRevision);
        card["previousWorldKept"] = report->outcome != GenerationOutcome::Success && s.LastRunKeptPreviousWorld();
        mCard = card;
        if (s.IsImported())
        {
            mCardImportRevision = s.Revision();   // this import is already superseded by (or older than) the report
        }
    }
    else if (importNewer)
    {
        mSeenReportRevision = s.ReportRevision();
        mCardImportRevision = s.Revision();
        Json::Value card(Json::objectValue);
        card["kind"] = "import";
        card["outcome"] = "Imported";
        card["name"] = mImportNameRevision == s.Revision() ? mImportName : std::string();
        card["revision"] = static_cast<Json::UInt>(s.Revision());
        card["seed"] = static_cast<Json::UInt64>(s.Seed());
        card["previousWorldKept"] = false;
        mCard = card;
    }
    else if (mCard.isObject() && mCard["kind"].asString() == "import" && !s.IsImported())
    {
        mCard = Json::Value(Json::nullValue);   // a live preview replaced the imported world
    }
}

Json::Value WorldGenDebugDomain::BuildValidation() const
{
    const WorldGenSession& s = mModule.GetSession();
    const GenerationReport* report = s.LastReport();
    const bool freshEvidence = s.HasEvidence() && s.EvidenceRevision() == s.Revision();

    const ValidationResult* validation = nullptr;
    bool fromEvidence = false;
    if (freshEvidence)
    {
        validation = &s.Evidence();
        fromEvidence = true;
    }
    else if (report != nullptr)
    {
        validation = &report->finalValidation;
    }
    else if (s.HasEvidence())
    {
        validation = &s.Evidence();   // stale: the world moved on since Validate
        fromEvidence = true;
    }
    if (validation == nullptr)
    {
        return Json::Value(Json::nullValue);
    }

    Json::Value v(Json::objectValue);
    v["outcome"] = report != nullptr ? OutcomeText(report->outcome) : "Validated";
    v["seed"] = static_cast<Json::UInt64>(report != nullptr ? report->outerSeed : s.Seed());
    const uint32_t worldRevision = fromEvidence ? s.EvidenceRevision() : report->worldRevision;
    v["worldRevision"] = static_cast<Json::UInt>(worldRevision);
    v["fresh"] = worldRevision != 0 && worldRevision == s.Revision();
    v["passed"] = validation->passed;
    v["source"] = fromEvidence ? "validate" : "generation";

    Json::Value rows(Json::arrayValue);
    for (unsigned i = 0; i < validation->guarantees.Size(); ++i)
    {
        const GuaranteeResult& g = validation->guarantees[i];
        Json::Value e(Json::objectValue);
        e["id"] = static_cast<Json::UInt>(g.idCrc);
        e["name"] = WorldGenSession::GuaranteeLabel(g.idCrc);
        e["ok"] = g.satisfied;
        Json::Value counts(Json::objectValue);
        counts["proofPaths"] = static_cast<Json::UInt>(g.proofPaths.Size());
        counts["violatingCells"] = static_cast<Json::UInt>(g.violatingCells.Size());
        counts["violatingPairs"] = static_cast<Json::UInt>(g.violatingPairs.Size());
        e["counts"] = counts;
        Json::Value diags(Json::arrayValue);
        for (unsigned d = 0; d < g.diagnostics.Size() && d < 4; ++d)
        {
            diags.append(Clip(g.diagnostics[d].text, kMaxTextChars));
        }
        e["diagnostics"] = diags;
        rows.append(e);
    }
    v["guarantees"] = rows;

    Json::Value attempts(Json::arrayValue);
    const unsigned attemptCount = report != nullptr ? report->attempts.Size() : 0u;
    for (unsigned i = 0; i < attemptCount && i < kMaxAttemptRows; ++i)
    {
        const AttemptRecord& a = report->attempts[i];
        Json::Value e(Json::objectValue);
        e["index"] = static_cast<Json::UInt>(a.attempt);
        e["subSeed"] = static_cast<Json::UInt64>(a.subSeed);
        Json::Value failed(Json::arrayValue);
        for (uint32_t f = 0; f < a.failedGuaranteeCount; ++f)
        {
            failed.append(WorldGenSession::GuaranteeLabel(a.failedGuarantees[f]));
        }
        e["failedGuarantees"] = failed;
        e["ms"] = static_cast<double>(a.elapsedMs);
        attempts.append(e);
    }
    v["attemptCount"] = static_cast<Json::UInt>(attemptCount);
    v["attempts"] = attempts;
    return v;
}

Json::Value WorldGenDebugDomain::BuildFile(bool full)
{
    const WorldGenSession& s = mModule.GetSession();
    if (!mWorldsBuilt || mWorldsDirty || full ||
        std::chrono::steady_clock::now() - mWorldsBuiltAt >= std::chrono::seconds(kStaticRefreshSeconds))
    {
        RefreshWorlds();
    }

    Json::Value f(Json::objectValue);
    f["worlds"] = mWorlds;
    f["selected"] = mSelectedWorld;
    f["defaultName"] = DefaultWorldName(s.Revision() > 0 ? s.World().provenance.seed : s.Seed());
    f["lastExport"] = mLastExport;
    f["lastImport"] = mLastImport;
    f["locked"] = s.IsLocked();
    f["imported"] = s.IsImported();
    if (s.Revision() > 0)
    {
        const WorldProvenance& p = s.World().provenance;
        Json::Value prov(Json::objectValue);
        prov["seed"] = static_cast<Json::UInt64>(p.seed);
        prov["generatorVersion"] = static_cast<Json::UInt>(p.generatorVersion);
        prov["rulesVersion"] = static_cast<Json::UInt>(p.rulesVersion);
        prov["preset"] = std::string(p.presetId);
        f["origin"] = prov;
    }
    return f;
}

Json::Value WorldGenDebugDomain::BuildProgress()
{
    const WorldGenSession& s = mModule.GetSession();
    Json::Value p(Json::objectValue);
    const bool generating = s.IsGenerating();
    std::string state = WorldGenStateName(s.State());
    if (generating)
    {
        state = "Generating";
    }
    else if (s.State() == WorldGenState::Failed && s.LastRunKeptPreviousWorld())
    {
        state = "Complete";   // the previous world is still the current one; the card carries the failure
    }
    p["state"] = state;
    p["attempt"] = 0;
    p["maxAttempts"] = 0;
    p["elapsedMs"] = 0;
    p["timeoutMs"] = 0;
    if (generating)
    {
        const Json::Value progress = s.Status()["progress"];
        p["attempt"] = progress["attempts"];
        p["maxAttempts"] = progress["maxAttempts"];
        p["elapsedMs"] = progress["elapsedSeconds"].asDouble() * 1000.0;
        p["timeoutMs"] = progress["timeoutSeconds"].asDouble() * 1000.0;
    }
    p["lastOutcome"] = mCard;
    return p;
}

void WorldGenDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    WorldGenSession& session = mModule.GetSession();
    const bool hasObj = args.isObject();
    bool acted = false;

    if (cmd == kCmdSetScale)
    {
        if (hasObj && args["key"].isString() && args.isMember("value"))
        {
            Json::Value set(Json::objectValue);
            set[args["key"].asString()] = args["value"];
            CarryUnlock(args, set);
            WarnIfFailed("setScale", session.Set(set));
            acted = true;
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: setScale needs {key, value}");
        }
    }
    else if (cmd == kCmdRegenerate)
    {
        WarnIfFailed("regenerate", session.Regenerate(hasObj ? args : Json::Value(Json::objectValue)));
        acted = true;
    }
    else if (cmd == kCmdSetSeed)
    {
        if (hasObj && args.isMember("seed"))
        {
            Json::Value set(Json::objectValue);
            set["seed"] = args["seed"];
            CarryUnlock(args, set);
            WarnIfFailed("setSeed", session.Set(set));
            acted = true;
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: setSeed needs {seed}");
        }
    }
    else if (cmd == kCmdSetSize)
    {
        WarnIfFailed("setSize", session.SetSize(args));
        acted = true;
    }
    else if (cmd == kCmdPresetLoad)
    {
        WarnIfFailed("presetLoad", session.PresetLoad(args));
        acted = true;
    }
    else if (cmd == kCmdPresetSave)
    {
        mLastSave = session.PresetSave(args);
        WarnIfFailed("presetSave", mLastSave);
        mStaticDirty = true;   // the preset list may have changed
        acted = true;
    }
    else if (cmd == kCmdSelectCell)
    {
        if (hasObj && args["x"].isIntegral() && args["y"].isIntegral() && args["x"].isInt() && args["y"].isInt())
        {
            if (mView != nullptr)
            {
                if (!mView->SelectCell(args["x"].asInt(), args["y"].asInt()))
                {
                    DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: selectCell (%d, %d) is outside the viewer grid", args["x"].asInt(), args["y"].asInt());
                }
            }
            else
            {
                mHasSelection = true;
                mSelX = args["x"].asInt();
                mSelY = args["y"].asInt();
            }
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: selectCell needs integer {x, y}");
        }
    }
    else if (cmd == kCmdClearCell)
    {
        if (mView != nullptr)
        {
            mView->ClearSelection();
        }
        else
        {
            mHasSelection = false;
        }
    }
    else if (cmd == kCmdSetColourMode)
    {
        if (mView != nullptr)
        {
            // "terrain" is the panel's name for the viewer's default base layer; the viewer accepts the alias.
            if (!(hasObj && args["mode"].isString() && mView->SetColourMode(args["mode"].asCString())))
            {
                DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: setColourMode needs a known {mode}");
            }
        }
        else if (hasObj && args["mode"].isString() && IsKnownMode(session, args["mode"].asString()))
        {
            mColourMode = args["mode"].asString();
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: setColourMode needs a known {mode}");
        }
    }
    else if (cmd == kCmdToggle)
    {
        bool found = false;
        if (mView != nullptr)
        {
            if (hasObj && args["drawer"].isString())
            {
                const std::string name = args["drawer"].asString();
                mView->ListOverlays([&](const char* id, bool enabled, bool available) {
                    if (!found && available && name == id)
                    {
                        found = mView->SetOverlayEnabled(id, !enabled);
                    }
                });
            }
        }
        else if (hasObj && args["drawer"].isString())
        {
            for (Drawer& d : mDrawers)
            {
                if (args["drawer"].asString() == d.name && d.available)
                {
                    d.enabled = !d.enabled;
                    found = true;
                }
            }
        }
        if (!found)
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: toggle needs an available {drawer}");
        }
    }
    else if (cmd == kCmdExpandGuarantee)
    {
        // {id} = a guarantee id from validation.guarantees[].id; missing, 0 or non-numeric collapses.
        if (mView != nullptr)
            mView->ExpandGuarantee(hasObj && args["id"].isUInt() ? args["id"].asUInt() : 0u);
    }
    else if (cmd == kCmdValidate)
    {
        const Json::Value r = session.Validate(Json::Value(Json::objectValue));
        WarnIfFailed("validate", r);
        mValidateError = r["success"].asBool() ? std::string() : (r["error"].isString() ? r["error"].asString() : std::string("validate failed"));
        acted = true;
    }
    else if (cmd == kCmdExport)
    {
        if (args.isNull() || hasObj)
        {
            mLastExport = CompactFileResult(mModule.ExportWorldFile(hasObj ? args : Json::Value(Json::objectValue)));
            mWorldsDirty = true;
            acted = true;
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: export needs {name?, overwrite?}");
        }
    }
    else if (cmd == kCmdImport)
    {
        if (hasObj && args["name"].isString())
        {
            const Json::Value r = mModule.ImportWorldFile(args);
            mLastImport = CompactFileResult(r);
            if (r["success"].asBool())
            {
                mImportName = args["name"].asString();
                mImportNameRevision = session.Revision();
            }
            acted = true;
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: import needs a {name} string");
        }
    }
    else if (cmd == kCmdCancel)
    {
        WarnIfFailed("cancel", session.Cancel(Json::Value(Json::objectValue)));
        acted = true;
    }
    else if (cmd == kCmdSelectWorld)
    {
        RefreshWorlds();
        bool found = false;
        if (hasObj && args["name"].isString())
        {
            for (const Json::Value& w : mWorlds)
            {
                if (w["name"].asString() == args["name"].asString())
                {
                    mSelectedWorld = args["name"].asString();
                    found = true;
                }
            }
        }
        if (!found)
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: selectWorld needs the {name} of a listed world");
        }
    }
    else if (cmd == kCmdDiscard)
    {
        const bool plain = args.isNull() || (hasObj && args.empty());
        if (plain)
        {
            if (session.IsLocked())
            {
                Json::Value set(Json::objectValue);
                set["unlock"] = true;
                WarnIfFailed("discard", session.Set(set));
                acted = true;
            }
        }
        else if (hasObj && args["cmd"].isString() && args["args"].isObject())
        {
            const Dia::Core::StringCRC pending(args["cmd"].asCString());
            if (pending == kCmdSetScale || pending == kCmdSetSeed || pending == kCmdSetSize || pending == kCmdPresetLoad || pending == kCmdRegenerate)
            {
                Json::Value replay = args["args"];
                replay["unlock"] = true;
                OnCommand(pending, replay);   // the same handler the panel's own command uses, now carrying unlock
            }
            else
            {
                DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: discard cannot replay that command");
            }
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: discard needs no args or {cmd, args}");
        }
    }
    else
    {
        DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: unknown command ignored");
    }

    if (acted)
    {
        mModule.NotifyAction();
    }
}

} // namespace CoW

#endif // DIA_DEBUG
