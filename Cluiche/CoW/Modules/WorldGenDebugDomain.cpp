#include "Modules/WorldGenDebugDomain.h"
#ifdef DIA_DEBUG

#include "Modules/WorldGenerationModule.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <algorithm>
#include <cctype>
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

    const char* const kDefaultMode = "terrain";

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

Dia::Core::StringCRC WorldGenDebugDomain::GetDomainId()    const { return Dia::Core::StringCRC("WorldGen"); }
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

    // view modes
    next.modes = Json::Value(Json::arrayValue);
    next.modes.append(kDefaultMode);
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        next.modes.append(TerrainLayers::GetLayerInfo(i).name);
    }

    for (int i = 0; i < kImplementedStageCount; ++i)
    {
        next.timingNames.push_back(TerrainStageName(static_cast<TerrainStage>(i)));
    }

    const bool changed = !mStaticBuilt || !(next.controlDefs == mStatic.controlDefs) || next.controlKeys != mStatic.controlKeys ||
                         !(next.presets == mStatic.presets) || !(next.modes == mStatic.modes) || next.timingNames != mStatic.timingNames;
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
    stats["locked"]         = false;
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
    view["colourMode"] = mColourMode;
    if (full)
    {
        view["modes"] = mStatic.modes;
    }
    Json::Value drawers(Json::arrayValue);
    for (const Drawer& d : mDrawers)
    {
        Json::Value e(Json::objectValue);
        e["name"] = d.name;
        e["enabled"] = d.enabled;
        e["available"] = d.available;
        drawers.append(e);
    }
    view["drawers"] = drawers;

    // cell inspector
    out["cell"] = Json::Value(Json::nullValue);
    if (mHasSelection)
    {
        Json::Value args(Json::objectValue);
        args["x"] = mSelX;
        args["y"] = mSelY;
        Json::Value cell = session.Cell(args);
        if (cell.isObject() && cell["success"].asBool())
        {
            cell["selected"]["x"] = mSelX;
            cell["selected"]["y"] = mSelY;
            out["cell"] = cell;
        }
        else
        {
            out["cellError"] = cell.isMember("error") ? cell["error"] : Json::Value("no cell");
        }
    }

    out["lastSave"] = mLastSave;
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
            // TODO(plan task 10): route click selection through DiaCellLayers cells.select.cell.
            mHasSelection = true;
            mSelX = args["x"].asInt();
            mSelY = args["y"].asInt();
        }
        else
        {
            DIA_LOG_WARNING("CoW", "WorldGenDebugDomain: selectCell needs integer {x, y}");
        }
    }
    else if (cmd == kCmdClearCell)
    {
        mHasSelection = false;
    }
    else if (cmd == kCmdSetColourMode)
    {
        if (hasObj && args["mode"].isString() && IsKnownMode(session, args["mode"].asString()))
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
        if (hasObj && args["drawer"].isString())
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
