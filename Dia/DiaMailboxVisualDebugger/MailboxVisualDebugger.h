////////////////////////////////////////////////////////////////////////////////
// Filename: MailboxVisualDebugger.h
// Description: IDebugDomain implementation for the Mailbox subsystem.
//              Panel-only domain — exposes per-type queue stats, fill levels,
//              send/drop/drain counters via GetJSONState(). No world-space drawers.
// System spec: docs/specs/applications/dia/systems/diamailboxvisualdebugger/diamailboxvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia::Mailbox { class Mailbox; }

namespace Dia::Mailbox
{

class MailboxVisualDebugger : public Dia::VisualDebugger::IDebugDomain
{
public:
    // mailbox must outlive this object.
    explicit MailboxVisualDebugger(const Mailbox& mailbox);

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

private:
    const Mailbox& mMailbox;
    std::atomic<bool> mQueueTableEnabled{true};
    std::atomic<bool> mDropAlertsEnabled{true};
};

} // namespace Dia::Mailbox

#endif // DIA_DEBUG
