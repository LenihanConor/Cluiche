#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>

namespace Dia { namespace Economy {
    class EconomySystem;
} }

namespace Cluiche { namespace AppFlow {

// Broadcasts economy.modifiers — active modifier stack per instance per resource.
// Fires only when the modifier stack hash changes (change-detected strategy).
class EconomyModifiersSource : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit EconomyModifiersSource(Dia::Economy::EconomySystem& system);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    Dia::Economy::EconomySystem& mSystem;
    unsigned int                 mFrame = 0;
};

}} // namespace Cluiche::AppFlow
