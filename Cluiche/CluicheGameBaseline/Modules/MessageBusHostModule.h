////////////////////////////////////////////////////////////////////////////////
// Filename: MessageBusHostModule.h
// Description: Manifest-loadable DiaMessageBus. A MessageBusModule that games
//              can list in their .diaapp (MessageBusModule itself lives in Dia
//              and registers nothing). In Debug builds it also registers a
//              MessageBusDebugDomain with VisualDebuggerModule so the bus
//              appears as a card in the `~` panel.
//              Other modules reach the Bus with ModuleRef<MessageBusHostModule>.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/MessageBusModule.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#endif

namespace Dia::MessageBus { class MessageBusDebugDomain; }

namespace Cluiche { namespace AppFlow {

class MessageBusHostModule : public Dia::MessageBus::MessageBusModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Hosts the DiaMessageBus Bus and shows it in the debug panel";

    explicit MessageBusHostModule(const Dia::Core::StringCRC& instanceId);
    ~MessageBusHostModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
#ifdef DIA_DEBUG
    // Registers the debug domain once the VisualDebuggerModule is available.
    void TryRegisterDebugDomain();
    void UnregisterDebugDomain();

    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Dia::MessageBus::MessageBusDebugDomain> mDebugDomain;
    bool mDomainRegistered = false;
#endif
};

} } // namespace Cluiche::AppFlow
