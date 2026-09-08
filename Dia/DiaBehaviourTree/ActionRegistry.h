#pragma once
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace BehaviourTree {
    using ActionFn = NodeResult(*)(void* actionContext,
                                   const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params);

    class ActionRegistry {
    public:
        ActionRegistry();
        ~ActionRegistry();

        void Register(Dia::Core::StringCRC actionId, ActionFn fn);
        ActionFn Find(Dia::Core::StringCRC actionId) const;
        bool Has(Dia::Core::StringCRC actionId) const;

    private:
        struct Impl;
        Impl* mImpl;
    };
} }
