#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity {

    class Domain;

    class IComponent {
    public:
        virtual ~IComponent() = default;

        // Called once after construction and field load, before any DoUpdate call.
        virtual void OnAttach(Domain& domain, Entity self) {}

        // Called once before destruction. The component is still attached and queryable.
        virtual void OnDetach(Domain& domain, Entity self) {}

        // Optional per-frame tick. Components opt in by overriding.
        virtual void DoUpdate(Domain& domain, Entity self, float dt) {}

        // Called when a referenced asset has finished loading.
        virtual void OnAssetLoaded(Domain& domain, Entity self, Dia::Core::StringCRC assetId) {}

        // Identity — filled in by DIA_COMPONENT macro.
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
    };

} // namespace Dia::Entity
