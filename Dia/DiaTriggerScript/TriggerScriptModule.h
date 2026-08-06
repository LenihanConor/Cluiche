#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaTriggerScript/TriggerActionRegistry.h>
#include <DiaTriggerScript/TriggerDef.h>
#include <DiaTriggerScript/TriggerFiredEvent.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace EntitySpatial { class EntitySpatialModule; } }

namespace Dia
{
    namespace TriggerScript
    {
        //-------------------------------------------------------------------------------------------
        // TriggerScriptModule
        //
        // ApplicationFlow::Module on SimPU. Owns a flat list of TriggerDefs for the current level,
        // polls them each tick, and dispatches actions when conditions fire.
        //
        // SD-001: Module-owned flat trigger list — no entity anchor.
        // SD-002: Tick-polled with per-trigger checkIntervalMs throttle.
        // SD-007: One-shot is the default; repeating is opt-in.
        //-------------------------------------------------------------------------------------------
        class TriggerScriptModule : public Dia::ApplicationFlow::Module
        {
        public:
            static const Dia::Core::StringCRC kInstanceId;

            TriggerScriptModule();
            ~TriggerScriptModule() override;

            TriggerScriptModule(const TriggerScriptModule&) = delete;
            TriggerScriptModule& operator=(const TriggerScriptModule&) = delete;

            // Load trigger list from JSON. Validates all state-trigger ConditionExprs.
            // Returns false and populates outErrors on any failure.
            // Must be called before DoStart().
            bool LoadFromJson(
                const Json::Value& root,
                const Dia::Condition::ConditionRegistry& validationRegistry,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

            // Set dependencies — must all be called before DoStart().
            void SetActionRegistry(TriggerActionRegistry* registry);
            void SetConditionContext(Dia::Condition::IConditionContext* ctx);
            void SetSpatialModule(Dia::EntitySpatial::EntitySpatialModule* spatial);

            // Push API for count triggers (SD-ODP-1 resolution).
            // Game code calls this on kill/spawn events; the module increments
            // internalCount for all count triggers whose entityTag matches.
            void IncrementCount(Dia::Core::StringCRC entityTag, int delta = 1);

            // Drive the trigger evaluation loop directly.
            // Called by DoUpdate each frame; also exposed for unit tests that
            // run without a full ApplicationFlow lifecycle.
            void Tick(float deltaTime);

            // Inspection
            int  GetTriggerCount() const;
            bool IsFired (Dia::Core::StringCRC triggerId) const;
            bool IsActive(Dia::Core::StringCRC triggerId) const;

        protected:
            void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
            Dia::ApplicationFlow::StartResult DoStart() override;
            void DoUpdate(float deltaTime) override;
            Dia::ApplicationFlow::StopResult  DoStop() override;

        private:
            struct Impl;
            Impl* mImpl;

            Dia::ApplicationFlow::EventStreamWriter<TriggerFiredEvent>
                mFiredWriter{this, Dia::Core::StringCRC("triggerscript.trigger-fired")};
        };

    } // namespace TriggerScript
} // namespace Dia
