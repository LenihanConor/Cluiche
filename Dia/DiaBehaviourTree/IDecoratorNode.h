#pragma once
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace BehaviourTree {
    struct DecoratorContext {
        float  deltaTime;
        int&   counter;      // per-entity-per-node int (owned by BehaviourTreeComponent cursor)
        float& accumulator;  // per-entity-per-node float (owned by BehaviourTreeComponent cursor)
    };

    class IDecoratorNode {
    public:
        virtual ~IDecoratorNode() = default;
        virtual bool ShouldTickChild(const DecoratorContext& ctx) const = 0;
        virtual NodeResult Evaluate(NodeResult childResult, DecoratorContext& ctx) const = 0;
        virtual void OnReset(DecoratorContext& ctx) const = 0;
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
    };
} }
