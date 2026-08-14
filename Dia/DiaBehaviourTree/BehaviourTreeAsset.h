#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <vector>

namespace Dia
{
    namespace BehaviourTree
    {
        //-------------------------------------------------------------------------------------------
        // BehaviourTreeAsset
        //
        // Immutable after LoadFromJson. Shared safely across entities running the same tree.
        //
        // SD-005: Immutable after LoadFromJson().
        // SD-009: Caller owns JSON parsing; LoadFromJson takes const Json::Value&.
        // PD-001: All node/action/type IDs use StringCRC.
        // PD-004: No STL in public API — DynamicArrayC for error output.
        // AD-003: Namespace Dia::BehaviourTree::.
        //-------------------------------------------------------------------------------------------

        class BehaviourTreeAsset
        {
        public:
            // Descriptor for a single node in the behaviour tree.
            // Public so the execution engine can read its fields.
            struct NodeDescriptor
            {
                Dia::Core::StringCRC id;
                Dia::Core::StringCRC type;          // "sequence", "selector", "parallel", "decorator", "action", "condition"

                // control-flow (sequence/selector/parallel)
                std::vector<Dia::Core::StringCRC> children;

                // parallel
                Dia::Core::StringCRC policy;        // "require_all", "require_one", "require_none"

                // decorator
                Dia::Core::StringCRC decoratorType; // "inverter", "repeater", "cooldown", "guard"
                Dia::Core::StringCRC childId;       // single child node ID
                int   repeatCount    = 0;
                bool  breakOnFailure = false;
                float cooldownSeconds = 0.0f;
                Dia::Core::StringCRC guardKey;      // blackboard key for guard decorator

                // action leaf
                Dia::Core::StringCRC actionId;
                std::vector<Dia::Core::StringCRC> params;

                // condition leaf
                Dia::Core::StringCRC blackboardKey;
            };

            BehaviourTreeAsset();
            ~BehaviourTreeAsset();

            BehaviourTreeAsset(BehaviourTreeAsset&&) noexcept;
            BehaviourTreeAsset& operator=(BehaviourTreeAsset&&) noexcept;

            BehaviourTreeAsset(const BehaviourTreeAsset&) = delete;
            BehaviourTreeAsset& operator=(const BehaviourTreeAsset&) = delete;

            // Load from JSON. Returns an invalid asset and populates errors on failure.
            static BehaviourTreeAsset LoadFromJson(
                const Json::Value& root,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

            // Validate: all child references resolvable, no cycles from root.
            bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            bool IsValid() const;

            Dia::Core::StringCRC GetRootNodeId() const;
            int GetNodeCount() const;

            // Returns nullptr if nodeId not found.
            const NodeDescriptor* GetNode(Dia::Core::StringCRC nodeId) const;
            bool HasNode(Dia::Core::StringCRC nodeId) const;

        private:
            struct Impl;
            Impl* mImpl;
            bool  mValid;
        };

    } // namespace BehaviourTree
} // namespace Dia
