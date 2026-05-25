#pragma once
#include <DiaCore/Json/external/json/json.h>

namespace Dia::Entity {
    class Domain;

    class IBlueprintLoader {
    public:
        virtual ~IBlueprintLoader() = default;

        // Load entities and queue component attachments from a blueprint JSON object.
        // Returns true if all entities were instantiated successfully.
        // Caller must call Domain::EndOfFrame() after Load to apply the queued mutations.
        virtual bool Load(Domain& domain, const Json::Value& blueprint) = 0;
    };

} // namespace Dia::Entity
