#include <Shared/Debug/RecordsCommandHelper.h>

#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {
namespace Debug {

RecordsCommandHelper::RecordsCommandHelper(const char* sysToken)
    : mName(World::CommandName(sysToken, "records"))
{
}

SimCommandEntry RecordsCommandHelper::BuildEntry()
{
    static const Dia::API::ValueParam kParams[] = {
        { "action", "string", true, "list | reload" } };

    SimCommandEntry entry;
    entry.name = mName.c_str();
    entry.kind = EntryKind::kCommand;
    entry.help.description = "Serve this system's record files: list the record ids, or queue a strict reload";
    entry.help.category = "worldsim";
    entry.help.owner = "RecordsCommandHelper";
    entry.help.params = kParams;
    entry.help.paramCount = sizeof(kParams) / sizeof(kParams[0]);
    entry.handler = [this](const Json::Value& args) { return Handle(args); };
    return entry;
}

Json::Value RecordsCommandHelper::Error(const char* text)
{
    Json::Value e(Json::objectValue);
    e["success"] = false;
    e["error"] = text;
    return e;
}

Json::Value RecordsCommandHelper::Handle(const Json::Value& args)
{
    if (!args.isObject() || !args["action"].isString()) { return Error("missing 'action' (list, reload)"); }
    const std::string action = args["action"].asString();
    if (action == "list")
    {
        if (!mListFn) { return Error("no record list wired, cannot list"); }
        Json::Value r(Json::objectValue);
        r["success"] = true;
        Json::Value ids(Json::arrayValue);
        for (const std::string& id : mListFn()) { ids.append(id); }
        r["records"] = ids;
        return r;
    }
    if (action == "reload")
    {
        if (!mEnqueue || !mReloadFn) { return Error("no reload wired, cannot reload"); }
        if (!mEnqueue()) { return Error("mutation queue full, try again after the next step"); }
        Json::Value r(Json::objectValue);
        r["success"] = true;
        r["queued"] = true;
        return r;
    }
    return Error(("unknown action: '" + action + "'").c_str());
}

bool RecordsCommandHelper::ApplyReload()
{
    if (!mReloadFn) { return false; }
    const RecordReloadResult result = mReloadFn();
    if (result.ok)
    {
        if (mHealth) { mHealth(false, std::string()); }
        return true;
    }
    const std::string text = "record reload failed: file '" + result.error.file + "' record '" + result.error.record +
                             "' field '" + result.error.field + "': " + result.error.text;
    DIA_LOG_ERROR("WorldSim", "%s: %s", mName.c_str(), text.c_str());
    if (mHealth) { mHealth(true, text); }
    return false;
}

}  // namespace Debug
}  // namespace CoW
