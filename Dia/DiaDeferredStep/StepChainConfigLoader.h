// Step chain config loader (JSON). Header-only template over the stage capacity.
// Spec: docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
#pragma once

#include <DiaDeferredStep/StepChainConfig.h>

#include <DiaCore/Json/external/json/json.h>

#include <string>

namespace Dia::DeferredStep
{
    namespace StepChainConfigDetail
    {
        inline Dia::Core::StringCRC DefaultHash(const char* text) { return Dia::Core::StringCRC(text); }

        template <uint32_t Capacity>
        bool Fail(StepChainConfig<Capacity>& out, ConfigError& err, const char* key, const char* reason)
        {
            out = StepChainConfig<Capacity>{};
            err.key.SetText(key);
            err.reason.SetText(reason);
            return false;
        }

        template <uint32_t Capacity>
        bool ParseOrder(const Json::Value& root, StageIdList<Capacity>& order, StageHashFn hash, ConfigError& err)
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
            if (arr.size() > Capacity) { return bad("too many stages"); }

            std::string seen[Capacity];
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

    // Collision seam: ids are hashed with `hash` (tests inject a colliding hash).
    // All-or-nothing: on any fault returns false, leaves out equal to a default config, and names the key in err.
    template <uint32_t Capacity = kDefaultStageCapacity>
    bool LoadStepChainConfig(const Json::Value& root, StepChainConfig<Capacity>& out, ConfigError& err, StageHashFn hash)
    {
        using namespace StepChainConfigDetail;
        out = StepChainConfig<Capacity>{};
        if (!root.isObject()) { return Fail(out, err, "root", "not an object"); }

        for (const std::string& name : root.getMemberNames())
        {
            if (name != "order" && name != "apply_after_steps" && name != "deferred")
            {
                return Fail(out, err, name.c_str(), "unknown key");
            }
        }

        StepChainConfig<Capacity> local;
        if (!ParseOrder<Capacity>(root, local.order, hash ? hash : &DefaultHash, err))
        {
            out = StepChainConfig<Capacity>{};
            return false;
        }

        if (root.isMember("apply_after_steps"))
        {
            const Json::Value& v = root["apply_after_steps"];
            if (!v.isInt()) { return Fail(out, err, "apply_after_steps", "not an integer"); }
            const int steps = v.asInt();
            if (steps < 1 || steps > 365) { return Fail(out, err, "apply_after_steps", "out of range 1..365"); }
            local.applyAfterSteps = static_cast<uint32_t>(steps);
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

    template <uint32_t Capacity = kDefaultStageCapacity>
    bool LoadStepChainConfig(const Json::Value& root, StepChainConfig<Capacity>& out, ConfigError& err)
    {
        return LoadStepChainConfig<Capacity>(root, out, err, &StepChainConfigDetail::DefaultHash);
    }
}
