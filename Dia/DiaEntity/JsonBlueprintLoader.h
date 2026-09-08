#pragma once
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/IBlueprintLoader.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {

    inline constexpr int kBlueprintSchemaVersion = 1;

    struct EntityNameHandle {
        Dia::Core::StringCRC name;
        Entity               entity;
    };

    class JsonBlueprintLoader final : public IBlueprintLoader {
    public:
        bool Load(Domain& domain, const Json::Value& blueprint) override;

    private:
        bool InstantiateEntities(
            Domain& domain,
            const Json::Value& entities,
            Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& outHandles);

        bool PatchReferences(
            Domain& domain,
            const Json::Value& references,
            const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& handles);

        bool ValidateReferences(
            Domain& domain,
            const Dia::Core::Containers::DynamicArrayC<EntityNameHandle, kMaxEntitiesPerDomain>& handles);
    };

} // namespace Dia::Entity
