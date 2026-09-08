#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaEconomy/EconomySchema.h>

namespace Dia { namespace Economy { class EconomySystem; } }

namespace Cluiche { namespace AppFlow {

// Broadcasts economy.schema — one-shot snapshot of the loaded EconomySchema:
// resource definitions, income rules, and cost tables.  Fires once on first
// connect (and once per new subscriber to handle reconnects).  No further
// updates are issued in v1 because the schema is immutable at runtime.
class EconomySchemaSource : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    EconomySchemaSource(const Dia::Economy::EconomySchema& schema,
                        const Dia::Economy::EconomySystem& system);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    const Dia::Economy::EconomySchema& mSchema;
    const Dia::Economy::EconomySystem& mSystem;
};

}} // namespace Cluiche::AppFlow
