#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaBlackboard/BlackboardRegistry.h>

namespace Cluiche { namespace AppFlow {

// Broadcasts blackboard.state — full snapshot of every registered board,
// its slots, and its observers. Fires only when structural hash changes
// (change-detected strategy).
class BlackboardInspectorSource : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit BlackboardInspectorSource(
        const Dia::Blackboard::BlackboardRegistry& registry);

    Dia::Core::StringCRC             GetTopic()  const override;
    Dia::DebugServer::SourcePolicy   GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    const Dia::Blackboard::BlackboardRegistry& mRegistry;
};

}} // namespace Cluiche::AppFlow
