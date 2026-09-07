////////////////////////////////////////////////////////////////////////////////
// Filename: MessageBusDebugDomain.h
// Description: IDebugDomain implementation for DiaMessageBus. Panel-only
//              (no world-space drawers) — exposes Schema / Live / History
//              tabs over Bus's routing table, last-tick ledger, and
//              LedgerHistory ring buffer to the in-game debug panel (`~`).
// System spec: docs/specs/applications/dia/systems/diamessagebus/visual-debugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaMessageBus/MessageBusModule.h>

namespace Dia::MessageBus {

    ////////////////////////////////////////////////////////////////////////////////
    // MessageBusDebugDomain
    //
    // Reads MessageBusModule's owned Bus and LedgerHistory in-process, on the
    // sim thread (this is the in-game Visual Debugger tier — no
    // DiaDebugServer, no cross-PU connection; see SD-MBX2-011). Contributes
    // no world-space drawers (HasWorldDrawers() == false); all state is
    // panel-only, delivered through GetJSONState().
    //
    //   - Schema tab  : Bus::ForEachRegisteredType/ForEachProducerForType/
    //                   ForEachSubscriberForType (routing graph metadata)
    //   - Live tab    : Bus::GetLastTickLedger() (last completed tick)
    //   - History tab : MessageBusModule::GetLedgerHistory() (ring buffer)
    ////////////////////////////////////////////////////////////////////////////////
    class MessageBusDebugDomain : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit MessageBusDebugDomain(const MessageBusModule& module);

        Dia::Core::StringCRC GetDomainId()     const override;
        const char*          GetDisplayName()  const override;
        const char*          GetDescription()  const override;
        Dia::Core::StringCRC GetGroup()        const override;
        Dia::Core::RGBA      GetAccentColour() const override;
        bool                 HasWorldDrawers() const override { return false; }

        // Register/Unregister/GetDrawerCount/GetDrawer intentionally left at
        // IDebugDomain's no-op defaults — this domain has no world drawers.

        void GetJSONState(Json::Value& out) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    private:
        enum class Tab : uint8_t { Schema, Live, History };

        static const char* TabName(Tab tab);

        void WriteSchemaTab(Json::Value& out)  const;
        void WriteLiveTab(Json::Value& out)    const;
        void WriteHistoryTab(Json::Value& out) const;

        const MessageBusModule& mModule;
        Tab                      mActiveTab          = Tab::Live;
        uint16_t                 mHistoryWindowTicks = 60;
    };

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
