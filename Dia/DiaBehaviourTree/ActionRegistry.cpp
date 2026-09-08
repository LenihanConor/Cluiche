#include "ActionRegistry.h"
#include <unordered_map>

namespace Dia { namespace BehaviourTree {
    struct ActionRegistry::Impl {
        std::unordered_map<unsigned int, ActionFn> table;
    };

    ActionRegistry::ActionRegistry()
        : mImpl(new Impl()) {
    }

    ActionRegistry::~ActionRegistry() {
        delete mImpl;
    }

    void ActionRegistry::Register(Dia::Core::StringCRC actionId, ActionFn fn) {
        mImpl->table[actionId.Value()] = fn;
    }

    ActionFn ActionRegistry::Find(Dia::Core::StringCRC actionId) const {
        const auto it = mImpl->table.find(actionId.Value());
        return it == mImpl->table.end() ? nullptr : it->second;
    }

    bool ActionRegistry::Has(Dia::Core::StringCRC actionId) const {
        return mImpl->table.count(actionId.Value()) != 0;
    }
} }
