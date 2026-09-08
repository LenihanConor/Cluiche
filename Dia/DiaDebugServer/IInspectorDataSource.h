#pragma once
////////////////////////////////////////////////////////////////////////////////
// IInspectorDataSource.h
//
// Narrow interface for inspector data feeds. Base classes in
// InspectorDataSourceBase.h handle "when to fire"; subclasses only provide
// "what to send."
//
// Dependency-free: only DiaCore types used. Implementations live in the
// application layer (e.g. CluicheGameBaseline/Modules/InspectorSources/).
////////////////////////////////////////////////////////////////////////////////
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace DebugServer {

class DebugServer;

enum class SourceStrategy : unsigned int
{
    kChangeDetected = 0, // fires when hashed state differs or subscriber count changes
    kPeriodic       = 1, // fires every periodSec; accumulates between broadcasts
    kEventDriven    = 2, // fires on push; base handles optional rate limiting
    kOnDemand       = 3, // responds to query; registered with QueryRegistry on Activate
};

struct SourcePolicy
{
    SourceStrategy strategy             = SourceStrategy::kChangeDetected;
    float          periodSec            = 0.0f;  // kPeriodic: broadcast interval
    int            maxEventsPerWindow   = 0;     // kEventDriven: rate cap (0 = unlimited)
    float          rateLimitWindowSec   = 1.0f;  // kEventDriven: window for cap
};

class IInspectorDataSource
{
public:
    virtual ~IInspectorDataSource() = default;

    virtual Dia::Core::StringCRC GetTopic()  const = 0;
    virtual SourcePolicy         GetPolicy() const = 0;

    // Called once when the server is ready. Cache server pointer here.
    virtual void Activate(DebugServer* server) = 0;

    // Called every host tick. connectionCount and subscriptionCount are passed
    // so sources can early-out without coupling to DebugServer internals.
    virtual void Tick(float deltaTime, int connectionCount, int subscriptionCount) = 0;

    // Called on shutdown. Release tap handles and any held resources.
    virtual void Deactivate() = 0;
};

}} // namespace Dia::DebugServer
