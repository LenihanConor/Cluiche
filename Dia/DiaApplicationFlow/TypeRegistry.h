#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <concepts>

namespace Dia { namespace ApplicationFlow {

    // C++20 concept: T must derive from Module and declare a static constexpr kTypeId.
    template<typename T>
    concept DerivedModule = std::derived_from<T, Module> && requires { T::kTypeId; };

    // ---------------------------------------------------------------------------
    // TypeRegistry
    //
    // Meyers-singleton global registry of Module factory functions, keyed by
    // StringCRC type ID.  Populated at static-init time via DIA_MODULE macros
    // through ModuleRegistration<T> — no manual registration call required.
    // ---------------------------------------------------------------------------
    class TypeRegistry
    {
    public:
        using FactoryFn = Module* (*)(const Dia::Core::StringCRC& instanceId);

        // Register a factory for a given typeId.  Duplicate registrations are
        // silently ignored (same behaviour as v1 ApplicationTypeRegistry).
        void Register(const Dia::Core::StringCRC& typeId, FactoryFn factory);

        // Create a Module instance by typeId.  Returns nullptr if typeId is
        // not registered.
        Module* Create(const Dia::Core::StringCRC& typeId,
                       const Dia::Core::StringCRC& instanceId) const;

        // Returns true if typeId has been registered.
        bool Contains(const Dia::Core::StringCRC& typeId) const;

        // Meyers singleton — thread-safe in C++11+.
        // Populated by DIA_MODULE static registrations before main() runs.
        static TypeRegistry& Global();

    private:
        // Initial capacity 32 payload slots, 64 hash-table buckets —
        // matches the module-factory table in v1 ApplicationTypeRegistry.
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, FactoryFn> mFactories{32, 64};
    };

    // ---------------------------------------------------------------------------
    // ModuleRegistration<T>
    //
    // Zero-cost static-init helper.  Instantiate one of these (via DIA_MODULE)
    // in a module's .cpp file to register it with the global TypeRegistry.
    // ---------------------------------------------------------------------------
    template<DerivedModule T>
    struct ModuleRegistration
    {
        explicit ModuleRegistration(const Dia::Core::StringCRC& typeId)
        {
            TypeRegistry::Global().Register(typeId,
                [](const Dia::Core::StringCRC& id) -> Module*
                {
                    return new T(id);
                });
        }
    };

}} // namespace Dia::ApplicationFlow
