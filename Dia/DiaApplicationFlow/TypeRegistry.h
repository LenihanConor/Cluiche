#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <concepts>
#include <type_traits>

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

        // Per-type metadata bundled alongside the factory.
        struct TypeMetadata
        {
            FactoryFn   factory     = nullptr;
            PUAffinity  allowedPUs  = PUAffinity::kAny;
            const char* description = nullptr;
        };

        // Register full metadata for a typeId.  Duplicate registrations are
        // silently ignored (same behaviour as v1 ApplicationTypeRegistry).
        void Register(const Dia::Core::StringCRC& typeId, const TypeMetadata& meta);

        // Backwards-compatible overload: register factory only (defaults apply).
        void Register(const Dia::Core::StringCRC& typeId, FactoryFn factory);

        // Create a Module instance by typeId.  Returns nullptr if typeId is
        // not registered.
        Module* Create(const Dia::Core::StringCRC& typeId,
                       const Dia::Core::StringCRC& instanceId) const;

        // Returns true if typeId has been registered.
        bool Contains(const Dia::Core::StringCRC& typeId) const;

        // Returns the allowed PU affinity mask for a registered type.
        // Returns PUAffinity::kAny if typeId is not found.
        PUAffinity GetAllowedPUs(const Dia::Core::StringCRC& typeId) const;

        // Returns the human-readable description for a registered type.
        // Returns nullptr if typeId is not found or no description was provided.
        const char* GetDescription(const Dia::Core::StringCRC& typeId) const;

        // Meyers singleton — thread-safe in C++11+.
        // Populated by DIA_MODULE static registrations before main() runs.
        static TypeRegistry& Global();

    private:
        // Initial capacity 32 payload slots, 64 hash-table buckets —
        // matches the module-factory table in v1 ApplicationTypeRegistry.
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, TypeMetadata> mFactories{32, 64};
    };

    // ---------------------------------------------------------------------------
    // SFINAE helpers — detect optional static constexpr members on module types.
    // ---------------------------------------------------------------------------
    template<typename T, typename = void>
    struct HasAllowedPUs : std::false_type {};
    template<typename T>
    struct HasAllowedPUs<T, std::void_t<decltype(T::kAllowedPUs)>> : std::true_type {};

    template<typename T, typename = void>
    struct HasDescription : std::false_type {};
    template<typename T>
    struct HasDescription<T, std::void_t<decltype(T::kDescription)>> : std::true_type {};

    // ---------------------------------------------------------------------------
    // ModuleRegistration<T>
    //
    // Zero-cost static-init helper.  Instantiate one of these (via DIA_MODULE)
    // in a module's .cpp file to register it with the global TypeRegistry.
    // Automatically picks up T::kAllowedPUs and T::kDescription if declared.
    // ---------------------------------------------------------------------------
    template<DerivedModule T>
    struct ModuleRegistration
    {
        explicit ModuleRegistration(const Dia::Core::StringCRC& typeId)
        {
            TypeRegistry::TypeMetadata meta;
            meta.factory = [](const Dia::Core::StringCRC& id) -> Module* { return new T(id); };
            if constexpr (HasAllowedPUs<T>::value)
                meta.allowedPUs = T::kAllowedPUs;
            if constexpr (HasDescription<T>::value)
                meta.description = T::kDescription;
            TypeRegistry::Global().Register(typeId, meta);
        }
    };

}} // namespace Dia::ApplicationFlow
