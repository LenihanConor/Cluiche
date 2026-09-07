#include "UtilityAIDebugDomain.h"

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::UtilityAI
{

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerScores("Scores");
}

UtilityAIDebugDomain::UtilityAIDebugDomain(const UtilitySet& utilitySet)
    : mUtilitySet(utilitySet)
{}

Dia::Core::StringCRC UtilityAIDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("UtilityAI"); }
const char* UtilityAIDebugDomain::GetDisplayName() const        { return "UtilityAI"; }
const char* UtilityAIDebugDomain::GetDescription() const        { return "Utility AI \xe2\x80\x94 per-action scores, winning action, score bars"; }
Dia::Core::StringCRC UtilityAIDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA UtilityAIDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void UtilityAIDebugDomain::GetJSONState(Json::Value& out)
{
    const bool enabled = mScoresEnabled.load();

    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "Scores";
        entry["enabled"] = enabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    // Fetch last-frame scores (populated after at least one Evaluate() call).
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> ids;
    Dia::Core::Containers::DynamicArrayC<float, 32> scores;
    mUtilitySet.GetLastFrameScores(ids, scores);

    const int count = static_cast<int>(ids.Size());

    // Sort indices descending by score (insertion sort; count is small).
    Dia::Core::Containers::DynamicArrayC<int, 32> order;
    for (int i = 0; i < count; ++i) order.Add(i);
    for (int i = 1; i < count; ++i)
    {
        const int key = order[i];
        int j = i - 1;
        while (j >= 0 && scores[order[j]] < scores[key])
        {
            order[j + 1] = order[j];
            --j;
        }
        order[j + 1] = key;
    }

    Json::Value stats(Json::objectValue);
    stats["actionCount"] = count;
    stats["winnerScore"] = (count > 0) ? scores[order[0]] : 0.0f;
    out["stats"] = stats;

    if (enabled)
    {
        Json::Value actions(Json::arrayValue);
        for (int rank = 0; rank < count; ++rank)
        {
            const int idx = order[rank];
            Json::Value a(Json::objectValue);
            a["id"]     = ids[idx].AsChar();
            a["score"]  = scores[idx];
            a["winner"] = (rank == 0);
            actions.append(a);
        }
        out["actions"] = actions;
    }
}

void UtilityAIDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        if (Dia::Core::StringCRC(args["drawer"].asCString()) == kDrawerScores)
            mScoresEnabled = !mScoresEnabled.load();
    }
    // "setScale" — no-op for panel-only domain
}

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
