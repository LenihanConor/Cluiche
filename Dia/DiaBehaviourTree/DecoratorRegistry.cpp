#include "DecoratorRegistry.h"
#include <unordered_map>

namespace Dia { namespace BehaviourTree {
    struct DecoratorRegistry::Impl {
        std::unordered_map<unsigned int, const IDecoratorNode*> table;
    };

    DecoratorRegistry::DecoratorRegistry()
        : mImpl(new Impl()) {
    }

    DecoratorRegistry::~DecoratorRegistry() {
        delete mImpl;
    }

    void DecoratorRegistry::Register(Dia::Core::StringCRC typeId, const IDecoratorNode* decorator) {
        mImpl->table[typeId.Value()] = decorator;
    }

    const IDecoratorNode* DecoratorRegistry::Find(Dia::Core::StringCRC typeId) const {
        const auto it = mImpl->table.find(typeId.Value());
        return it == mImpl->table.end() ? nullptr : it->second;
    }
} }
