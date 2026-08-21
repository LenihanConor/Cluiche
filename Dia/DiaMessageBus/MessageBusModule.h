#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/Bus.h>

namespace Dia::MessageBus {

// ---------------------------------------------------------------------------
// MessageBusModule
//
// Thin Dia::ApplicationFlow::Module wrapper around Bus. Owns one Bus
// instance and drives its flush from DoUpdate(). Follows the same
// implementation/module split as EntitySpawnerModule + EntitySpawnerImpl:
// almost all behaviour lives in Bus (directly unit-testable without the
// Module framework); this class only forwards the three lifecycle hooks.
// ---------------------------------------------------------------------------
class MessageBusModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kInstanceId;

    MessageBusModule();

    Bus&       GetBus();
    const Bus& GetBus() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    Bus mBus;
};

} // namespace Dia::MessageBus
