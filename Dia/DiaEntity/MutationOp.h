#pragma once
#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaEntity/Entity.h>

namespace Dia::Entity {

    enum class MutationKind : uint8_t {
        AddComponent,
        RemoveComponent,
        DestroyEntity,
        SetParent,
    };

    struct MutationOp {
        MutationKind         kind = MutationKind::AddComponent;
        Entity               entity;
        Dia::Core::StringCRC componentTypeId; // for AddComponent / RemoveComponent
        Json::Value          config;           // for AddComponent
        Entity               parentEntity;     // for SetParent
    };

} // namespace Dia::Entity
