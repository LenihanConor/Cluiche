#pragma once
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/ComponentTypeDesc.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {

    // Singleton registry that maps component type CRCs to ComponentTypeDesc entries.
    // Components register via DIA_COMPONENT_REGISTER at static-init time.
    class ComponentRegistry {
    public:
        static ComponentRegistry& Get();

        // Register a component type. Returns false if already registered or registry full.
        bool Register(const ComponentTypeDesc& desc);

        // Find by type CRC. Returns nullptr if not found.
        const ComponentTypeDesc* Find(Dia::Core::StringCRC typeId) const;

        uint32_t                 GetCount() const;
        const ComponentTypeDesc& GetByIndex(uint32_t i) const;

    private:
        Dia::Core::Containers::DynamicArrayC<const ComponentTypeDesc*, kMaxComponentTypesPerDomain> mDescs;
    };

} // namespace Dia::Entity
