////////////////////////////////////////////////////////////////////////////////
// Filename: TypeRegistry.cpp
// DiaApplicationFlow — v2 TypeRegistry implementation
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/TypeRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    TypeRegistry& TypeRegistry::Global()
    {
        // Meyers singleton: constructed on first call, destroyed at program exit.
        // Thread-safe in C++11 and later (static local init is sequenced).
        static TypeRegistry instance;
        return instance;
    }

    //--------------------------------------------------------------------------
    void TypeRegistry::Register(const Dia::Core::StringCRC& typeId, FactoryFn factory)
    {
        DIA_ASSERT(factory != nullptr, "TypeRegistry::Register — factory cannot be null");

        if (mFactories.ContainsKey(typeId))
        {
            DIA_LOG_WARNING("ApplicationFlow", "Module type '%s' already registered, skipping duplicate", typeId.AsChar());
            return;
        }

        mFactories.Add(typeId, factory);
    }

    //--------------------------------------------------------------------------
    Module* TypeRegistry::Create(const Dia::Core::StringCRC& typeId,
                                  const Dia::Core::StringCRC& instanceId) const
    {
        const FactoryFn* ppFactory = mFactories.TryGetItemConst(typeId);
        if (ppFactory == nullptr)
        {
            DIA_LOG_ERROR("ApplicationFlow", "Module type '%s' not registered in TypeRegistry", typeId.AsChar());
            return nullptr;
        }

        return (*ppFactory)(instanceId);
    }

    //--------------------------------------------------------------------------
    bool TypeRegistry::Contains(const Dia::Core::StringCRC& typeId) const
    {
        return mFactories.ContainsKey(typeId);
    }

}} // namespace Dia::ApplicationFlow
