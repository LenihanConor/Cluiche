#pragma once
#include <DiaBehaviourTree/IDecoratorNode.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace BehaviourTree {
    class DecoratorRegistry {
    public:
        DecoratorRegistry();
        ~DecoratorRegistry();

        void Register(Dia::Core::StringCRC typeId, const IDecoratorNode* decorator);
        const IDecoratorNode* Find(Dia::Core::StringCRC typeId) const;

    private:
        struct Impl;
        Impl* mImpl;
    };
} }
