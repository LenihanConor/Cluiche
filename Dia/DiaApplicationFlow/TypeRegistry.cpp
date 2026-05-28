////////////////////////////////////////////////////////////////////////////////
// Filename: TypeRegistry.cpp
// DiaApplicationFlow — v2 TypeRegistry implementation
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/TypeRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

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
    void TypeRegistry::Register(const Dia::Core::StringCRC& typeId, const TypeMetadata& meta)
    {
        DIA_ASSERT(meta.factory != nullptr, "TypeRegistry::Register — factory cannot be null");

        if (mFactories.ContainsKey(typeId))
        {
            DIA_LOG_WARNING("ApplicationFlow", "Module type '%s' already registered, skipping duplicate", typeId.AsChar());
            return;
        }

        mFactories.Add(typeId, meta);
    }

    //--------------------------------------------------------------------------
    void TypeRegistry::Register(const Dia::Core::StringCRC& typeId, FactoryFn factory)
    {
        TypeMetadata meta;
        meta.factory = factory;
        Register(typeId, meta);
    }

    //--------------------------------------------------------------------------
    Module* TypeRegistry::Create(const Dia::Core::StringCRC& typeId,
                                  const Dia::Core::StringCRC& instanceId) const
    {
        const TypeMetadata* pMeta = mFactories.TryGetItemConst(typeId);
        if (pMeta == nullptr)
        {
            DIA_LOG_ERROR("ApplicationFlow", "Module type '%s' not registered in TypeRegistry", typeId.AsChar());
            return nullptr;
        }

        return pMeta->factory(instanceId);
    }

    //--------------------------------------------------------------------------
    bool TypeRegistry::Contains(const Dia::Core::StringCRC& typeId) const
    {
        return mFactories.ContainsKey(typeId);
    }

    //--------------------------------------------------------------------------
    PUAffinity TypeRegistry::GetAllowedPUs(const Dia::Core::StringCRC& typeId) const
    {
        const TypeMetadata* pMeta = mFactories.TryGetItemConst(typeId);
        if (pMeta == nullptr)
            return PUAffinity::kAny;
        return pMeta->allowedPUs;
    }

    //--------------------------------------------------------------------------
    const char* TypeRegistry::GetDescription(const Dia::Core::StringCRC& typeId) const
    {
        const TypeMetadata* pMeta = mFactories.TryGetItemConst(typeId);
        if (pMeta == nullptr)
            return nullptr;
        return pMeta->description;
    }

}} // namespace Dia::ApplicationFlow
