#include <Shared/WorldStep/WorldStepConfig.h>

#include <DiaCore/Json/external/json/json.h>

#include <string>

namespace CoW::WorldStep
{
    namespace
    {
        using Dia::Core::StringCRC;

        StringCRC DefaultHash(const char* text) { return StringCRC(text); }

        bool Fail(WorldStepConfig& out, ConfigError& err, const char* key, const char* reason)
        {
            out = WorldStepConfig{};
            err.key.SetText(key);
            err.reason.SetText(reason);
            return false;
        }

        bool ParseOrder(const Json::Value& root, StageIdList& order, StageHashFn hash, ConfigError& err)
        {
            auto bad = [&](const char* reason)
            {
                err.key.SetText("order");
                err.reason.SetText(reason);
                return false;
            };

            if (!root.isMember("order")) { return bad("missing"); }
            const Json::Value& arr = root["order"];
            if (!arr.isArray()) { return bad("not an array"); }
            if (arr.empty()) { return bad("empty"); }
            if (arr.size() > kMaxStages) { return bad("too many stages"); }

            std::string seen[kMaxStages];
            uint32_t count = 0;
            for (const Json::Value& e : arr)
            {
                if (!e.isString()) { return bad("entry is not a string"); }
                const std::string text = e.asString();
                if (text.empty()) { return bad("empty id"); }
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (seen[i] == text) { return bad("duplicate id"); }
                }
                StageId id;
                id.crc = hash(text.c_str());
                id.name.SetText(text.c_str());
                for (uint32_t i = 0; i < order.Size(); ++i)
                {
                    if (order[i].crc == id.crc) { return bad("hash collision between ids"); }
                }
                seen[count++] = text;
                order.Add(id);
            }
            return true;
        }
    }

    bool LoadWorldStepConfig(const Json::Value& root, WorldStepConfig& out, ConfigError& err)
    {
        return LoadWorldStepConfig(root, out, err, &DefaultHash);
    }

    bool LoadWorldStepConfig(const Json::Value& root, WorldStepConfig& out, ConfigError& err, StageHashFn hash)
    {
        out = WorldStepConfig{};
        if (!root.isObject()) { return Fail(out, err, "root", "not an object"); }

        for (const std::string& name : root.getMemberNames())
        {
            if (name != "order" && name != "apply_after_days" && name != "deferred")
            {
                return Fail(out, err, name.c_str(), "unknown key");
            }
        }

        WorldStepConfig local;
        if (!ParseOrder(root, local.order, hash ? hash : &DefaultHash, err))
        {
            out = WorldStepConfig{};
            return false;
        }

        if (root.isMember("apply_after_days"))
        {
            const Json::Value& v = root["apply_after_days"];
            if (!v.isInt()) { return Fail(out, err, "apply_after_days", "not an integer"); }
            const int days = v.asInt();
            if (days < 1 || days > 365) { return Fail(out, err, "apply_after_days", "out of range 1..365"); }
            local.applyAfterDays = static_cast<uint32_t>(days);
        }

        if (root.isMember("deferred"))
        {
            const Json::Value& v = root["deferred"];
            if (!v.isBool()) { return Fail(out, err, "deferred", "not a bool"); }
            local.deferred = v.asBool();
        }

        out = local;
        return true;
    }
}
