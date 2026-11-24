////////////////////////////////////////////////////////////////////////////////
// Filename: MessageBusHostModule.h
// Description: Manifest-loadable DiaMessageBus. A SimModule owning the Bus
//              that games can list in their .diaapp. In Debug builds it also registers a
//              MessageBusDebugDomain with VisualDebuggerModule so the bus
//              appears as a card in the `~` panel.
//              Other modules reach the Bus with ModuleRef<MessageBusHostModule>.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/SimModule.h>
#include <DiaMessageBus/Bus.h>
#include <DiaMessageBus/BusHealthReporter.h>
#include <memory>

#ifdef DIA_DEBUG
#include <DiaMessageBus/LedgerHistory.h>
#include "Modules/VisualDebuggerModule.h"
#endif

namespace Dia::MessageBus { class MessageBusDebugDomain; }

namespace Cluiche { namespace AppFlow {

class MessageBusHostModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static const Dia::Core::StringCRC kInstanceId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Hosts the DiaMessageBus Bus and shows it in the debug panel";

    MessageBusHostModule();
    explicit MessageBusHostModule(const Dia::Core::StringCRC& instanceId);
    // Unregisters the health reporter if the module is destroyed without a Stop.
    ~MessageBusHostModule() override;

    Dia::MessageBus::Bus&       GetBus();
    const Dia::MessageBus::Bus& GetBus() const;

#ifdef DIA_DEBUG
    // Debug-only: last kLedgerCapacity completed-tick ledger snapshots, for a
    // future visual debugger's History tab. Absent entirely in Release.
    const Dia::MessageBus::LedgerHistory& GetLedgerHistory() const;
#endif

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::MessageBus::Bus mBus;
    Dia::MessageBus::BusHealthReporter mHealthReporter{Dia::Core::StringCRC("MessageBusModule"), mBus};
#ifdef DIA_DEBUG
    Dia::MessageBus::LedgerHistory mLedgerHistory;

    // Registers the debug domain once the VisualDebuggerModule is available.
    void TryRegisterDebugDomain();
    void UnregisterDebugDomain();

    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Dia::MessageBus::MessageBusDebugDomain> mDebugDomain;
    bool mDomainRegistered = false;
#endif
};

} } // namespace Cluiche::AppFlow
