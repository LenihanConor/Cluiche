#include "TriggerScriptModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaCore/Core/Assert.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaObservation/Log/DiaLog.h>

#include <vector>

namespace Dia
{
    namespace TriggerScript
    {
        // -----------------------------------------------------------------------------------------
        // TriggerRuntime — per-trigger mutable state, parallel to the TriggerDef array.
        // -----------------------------------------------------------------------------------------
        struct TriggerRuntime
        {
            bool  disabled        = false; // one-shot: true after first fire
            float accumulatedTime = 0.0f;  // temporal: seconds since last fire
            float timeSinceCheck  = 0.0f;  // spatial/state/count: seconds since last poll
            int   internalCount   = 0;     // count trigger: running tally via IncrementCount()
        };

        // -----------------------------------------------------------------------------------------
        // Impl
        // -----------------------------------------------------------------------------------------
        struct TriggerScriptModule::Impl
        {
            std::vector<TriggerDef>     defs;
            std::vector<TriggerRuntime> runtimes;

            TriggerActionRegistry*              actionRegistry   = nullptr;
            Dia::Condition::IConditionContext*   conditionContext  = nullptr;
            Dia::EntitySpatial::EntitySpatialModule* spatialModule = nullptr;
        };

        // -----------------------------------------------------------------------------------------
        // Static member
        // -----------------------------------------------------------------------------------------
        const Dia::Core::StringCRC TriggerScriptModule::kInstanceId{ "TriggerScriptModule" };

        // -----------------------------------------------------------------------------------------
        // Construction
        // -----------------------------------------------------------------------------------------
        TriggerScriptModule::TriggerScriptModule()
            : Dia::ApplicationFlow::Module(kInstanceId)
            , mImpl(new Impl())
        {
        }

        TriggerScriptModule::~TriggerScriptModule()
        {
            delete mImpl;
        }

        // -----------------------------------------------------------------------------------------
        // Configuration
        // -----------------------------------------------------------------------------------------
        void TriggerScriptModule::SetActionRegistry(TriggerActionRegistry* registry)
        {
            mImpl->actionRegistry = registry;
        }

        void TriggerScriptModule::SetConditionContext(Dia::Condition::IConditionContext* ctx)
        {
            mImpl->conditionContext = ctx;
        }

        void TriggerScriptModule::SetSpatialModule(Dia::EntitySpatial::EntitySpatialModule* spatial)
        {
            mImpl->spatialModule = spatial;
        }

        // -----------------------------------------------------------------------------------------
        // LoadFromJson
        // -----------------------------------------------------------------------------------------
        bool TriggerScriptModule::LoadFromJson(
            const Json::Value& root,
            const Dia::Condition::ConditionRegistry& validationRegistry,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
        {
            mImpl->defs.clear();
            mImpl->runtimes.clear();

            const Json::Value& triggersNode = root["triggers"];
            if (!triggersNode.isArray())
            {
                outErrors.Add("DiaTriggerScript: root must have a 'triggers' array");
                return false;
            }

            bool anyError = false;

            for (const Json::Value& node : triggersNode)
            {
                TriggerDef def;

                // id
                if (node.isMember("id") && node["id"].isString())
                    def.id = Dia::Core::StringCRC(node["id"].asCString());

                // one_shot (default true)
                def.oneShot = !node.isMember("one_shot") || node["one_shot"].asBool();

                // check_interval_ms (default 0)
                def.checkIntervalMs = node.isMember("check_interval_ms")
                    ? static_cast<float>(node["check_interval_ms"].asDouble())
                    : 0.0f;

                // type
                const char* typeStr = node.isMember("type") ? node["type"].asCString() : "";
                if      (strcmp(typeStr, "spatial")  == 0) def.type = TriggerType::kSpatial;
                else if (strcmp(typeStr, "temporal") == 0) def.type = TriggerType::kTemporal;
                else if (strcmp(typeStr, "state")    == 0) def.type = TriggerType::kState;
                else if (strcmp(typeStr, "count")    == 0) def.type = TriggerType::kCount;
                else
                {
                    outErrors.Add("DiaTriggerScript: unknown trigger type");
                    anyError = true;
                    continue;
                }

                // type-specific params
                if (def.type == TriggerType::kSpatial)
                {
                    if (node.isMember("region"))
                    {
                        const Json::Value& r = node["region"];
                        Dia::Maths::Vector2D minPt(
                            static_cast<float>(r["min"][0].asDouble()),
                            static_cast<float>(r["min"][1].asDouble()));
                        Dia::Maths::Vector2D maxPt(
                            static_cast<float>(r["max"][0].asDouble()),
                            static_cast<float>(r["max"][1].asDouble()));
                        def.spatial.region = Dia::Geometry2D::AARect(minPt, maxPt);
                    }
                    if (node.isMember("entity_tag") && node["entity_tag"].isString())
                        def.spatial.entityTag = Dia::Core::StringCRC(node["entity_tag"].asCString());
                }
                else if (def.type == TriggerType::kTemporal)
                {
                    def.temporal.intervalSeconds = node.isMember("interval_s")
                        ? static_cast<float>(node["interval_s"].asDouble())
                        : 0.0f;
                }
                else if (def.type == TriggerType::kState)
                {
                    if (node.isMember("condition"))
                    {
                        Dia::Core::Containers::DynamicArrayC<const char*, 32> exprErrors;
                        def.state.condition = Dia::Condition::ConditionExpr::LoadFromJson(
                            node["condition"], exprErrors);

                        if (!def.state.condition.IsValid())
                        {
                            outErrors.Add("DiaTriggerScript: invalid state condition expr");
                            anyError = true;
                            continue;
                        }

                        Dia::Core::Containers::DynamicArrayC<const char*, 32> valErrors;
                        if (!def.state.condition.Validate(validationRegistry, valErrors))
                        {
                            outErrors.Add("DiaTriggerScript: state condition failed validation");
                            anyError = true;
                            continue;
                        }
                    }
                }
                else if (def.type == TriggerType::kCount)
                {
                    if (node.isMember("entity_tag") && node["entity_tag"].isString())
                        def.count.entityTag = Dia::Core::StringCRC(node["entity_tag"].asCString());
                    def.count.threshold = node.isMember("threshold") ? node["threshold"].asInt() : 0;
                }

                // actions
                if (node.isMember("actions") && node["actions"].isArray())
                {
                    for (const Json::Value& actionNode : node["actions"])
                    {
                        if (def.actions.Size() >= 8) break;
                        ActionDef action;
                        if (actionNode.isMember("type") && actionNode["type"].isString())
                            action.actionType = Dia::Core::StringCRC(actionNode["type"].asCString());
                        if (actionNode.isMember("params"))
                            action.params = actionNode["params"];
                        def.actions.Add(std::move(action));
                    }
                }

                mImpl->defs.push_back(std::move(def));
                mImpl->runtimes.push_back(TriggerRuntime{});
            }

            return !anyError;
        }

        // -----------------------------------------------------------------------------------------
        // IncrementCount
        // -----------------------------------------------------------------------------------------
        void TriggerScriptModule::IncrementCount(Dia::Core::StringCRC entityTag, int delta)
        {
            for (size_t i = 0; i < mImpl->defs.size(); ++i)
            {
                const TriggerDef& def = mImpl->defs[i];
                if (def.type != TriggerType::kCount) continue;
                if (mImpl->runtimes[i].disabled) continue;
                if (def.count.entityTag == entityTag)
                    mImpl->runtimes[i].internalCount += delta;
            }
        }

        // -----------------------------------------------------------------------------------------
        // Inspection
        // -----------------------------------------------------------------------------------------
        int TriggerScriptModule::GetTriggerCount() const
        {
            return static_cast<int>(mImpl->defs.size());
        }

        bool TriggerScriptModule::IsFired(Dia::Core::StringCRC triggerId) const
        {
            for (size_t i = 0; i < mImpl->defs.size(); ++i)
            {
                if (mImpl->defs[i].id == triggerId)
                    return mImpl->defs[i].oneShot && mImpl->runtimes[i].disabled;
            }
            return false;
        }

        bool TriggerScriptModule::IsActive(Dia::Core::StringCRC triggerId) const
        {
            for (size_t i = 0; i < mImpl->defs.size(); ++i)
            {
                if (mImpl->defs[i].id == triggerId)
                    return !mImpl->runtimes[i].disabled;
            }
            return false;
        }

        // -----------------------------------------------------------------------------------------
        // Streams
        // -----------------------------------------------------------------------------------------
        void TriggerScriptModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
        {
            mFiredWriter.Connect(app);
        }

        // -----------------------------------------------------------------------------------------
        // Lifecycle
        // -----------------------------------------------------------------------------------------
        Dia::ApplicationFlow::StartResult TriggerScriptModule::DoStart()
        {
            DIA_ASSERT(mImpl->actionRegistry   != nullptr, "TriggerScriptModule: no action registry set");
            DIA_ASSERT(mImpl->conditionContext  != nullptr, "TriggerScriptModule: no condition context set");
            return Dia::ApplicationFlow::StartResult::kReady;
        }

        Dia::ApplicationFlow::StopResult TriggerScriptModule::DoStop()
        {
            return Dia::ApplicationFlow::StopResult::kDone;
        }

        // -----------------------------------------------------------------------------------------
        // Fire — dispatch all actions for a trigger and publish the event.
        // -----------------------------------------------------------------------------------------
        static void FireTrigger(
            const TriggerDef& def,
            TriggerRuntime& runtime,
            TriggerActionRegistry* actionRegistry,
            Dia::ApplicationFlow::EventStreamWriter<TriggerFiredEvent>& firedWriter)
        {
            // Dispatch actions in order
            for (int a = 0; a < def.actions.Size(); ++a)
            {
                const ActionDef& action = def.actions[a];
                if (actionRegistry->Has(action.actionType))
                {
                    ActionContext ctx{ def.id, action.params };
                    actionRegistry->Dispatch(action.actionType, ctx);
                }
                else
                {
                    DIA_LOG_WARNING("TriggerScript", "no handler for action type '%u', skipping", action.actionType.Value());
                }
            }

            // Publish event
            TriggerFiredEvent ev;
            ev.triggerId = def.id;
            ev.type      = def.type;
            firedWriter.Send(ev);

            // Re-arm or disable
            if (def.oneShot)
            {
                runtime.disabled = true;
            }
            else
            {
                runtime.accumulatedTime = 0.0f;
                runtime.timeSinceCheck  = 0.0f;
            }
        }

        // -----------------------------------------------------------------------------------------
        // Tick / DoUpdate
        // -----------------------------------------------------------------------------------------
        void TriggerScriptModule::DoUpdate(float deltaTime)
        {
            Tick(deltaTime);
        }

        void TriggerScriptModule::Tick(float deltaTime)
        {
            DIA_ASSERT(mImpl != nullptr, "TriggerScriptModule: null impl");

            const bool hasActionRegistry  = mImpl->actionRegistry  != nullptr;
            const bool hasConditionContext = mImpl->conditionContext != nullptr;

            if (!hasActionRegistry) return;

            for (size_t i = 0; i < mImpl->defs.size(); ++i)
            {
                const TriggerDef& def = mImpl->defs[i];
                TriggerRuntime&   rt  = mImpl->runtimes[i];

                if (rt.disabled) continue;

                switch (def.type)
                {
                // ----- Temporal -----
                case TriggerType::kTemporal:
                {
                    rt.accumulatedTime += deltaTime;
                    if (rt.accumulatedTime >= def.temporal.intervalSeconds)
                        FireTrigger(def, rt, mImpl->actionRegistry, mFiredWriter);
                    break;
                }

                // ----- State -----
                case TriggerType::kState:
                {
                    if (!hasConditionContext) break;

                    rt.timeSinceCheck += deltaTime;
                    const float thresholdSecs = def.checkIntervalMs / 1000.0f;
                    if (def.checkIntervalMs > 0.0f && rt.timeSinceCheck < thresholdSecs) break;
                    rt.timeSinceCheck = 0.0f;

                    if (def.state.condition.IsValid() &&
                        def.state.condition.Evaluate(*mImpl->conditionContext))
                    {
                        FireTrigger(def, rt, mImpl->actionRegistry, mFiredWriter);
                    }
                    break;
                }

                // ----- Spatial -----
                case TriggerType::kSpatial:
                {
                    if (!mImpl->spatialModule) break;

                    rt.timeSinceCheck += deltaTime;
                    const float thresholdSecs = def.checkIntervalMs / 1000.0f;
                    if (def.checkIntervalMs > 0.0f && rt.timeSinceCheck < thresholdSecs) break;
                    rt.timeSinceCheck = 0.0f;

                    // Query all entities in the region (layer mask 0xFFFFFFFF = all).
                    // Entity tag filtering deferred until Domain access is wired (ODP future).
                    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> results;
                    mImpl->spatialModule->QueryRegion(def.spatial.region, 0xFFFFFFFF, results);

                    if (results.Size() > 0)
                        FireTrigger(def, rt, mImpl->actionRegistry, mFiredWriter);
                    break;
                }

                // ----- Count -----
                case TriggerType::kCount:
                {
                    rt.timeSinceCheck += deltaTime;
                    const float thresholdSecs = def.checkIntervalMs / 1000.0f;
                    if (def.checkIntervalMs > 0.0f && rt.timeSinceCheck < thresholdSecs) break;
                    rt.timeSinceCheck = 0.0f;

                    if (rt.internalCount >= def.count.threshold)
                        FireTrigger(def, rt, mImpl->actionRegistry, mFiredWriter);
                    break;
                }
                }
            }
        }

    } // namespace TriggerScript
} // namespace Dia
