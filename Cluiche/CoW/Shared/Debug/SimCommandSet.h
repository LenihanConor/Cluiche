#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimQueryHost.h>

namespace CoW {
namespace Debug {

enum class EntryKind : uint8_t
{
    kCommand,
    kQuery,
};

// The registration surface SimCommandSet needs. The real adapter forwards to the Dia SimQueryHost; tests use a fake.
class ISimCommandHost
{
public:
    typedef Dia::DebugServer::SimQueryHost::QueryHandler Handler;
    virtual ~ISimCommandHost() {}
    virtual bool RegisterQuery(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo& help, Handler handler) = 0;
    virtual bool RegisterCommand(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo& help, Handler handler) = 0;
    // Kind-aware so a fake can assert it. Returns true if a name was removed.
    virtual bool Unregister(const Dia::Core::StringCRC& name, EntryKind kind) = 0;
};

// Adapter over the real SimQueryHost (UnregisterQuery erases by name whatever the kind).
class SimQueryHostAdapter : public ISimCommandHost
{
public:
    explicit SimQueryHostAdapter(Dia::DebugServer::SimQueryHost& host) : mHost(host) {}
    bool RegisterQuery(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo& help, Handler handler) override
    {
        return mHost.RegisterQuery(name, help, handler);
    }
    bool RegisterCommand(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo& help, Handler handler) override
    {
        return mHost.RegisterCommand(name, help, handler);
    }
    bool Unregister(const Dia::Core::StringCRC& name, EntryKind) override { return mHost.UnregisterQuery(name); }

private:
    Dia::DebugServer::SimQueryHost& mHost;
};

// One registerable command or query. Help is mandatory (Dia rejects an empty description).
struct SimCommandEntry
{
    const char* name = nullptr;
    EntryKind kind = EntryKind::kCommand;
    Dia::DebugServer::SimHelpInfo help;
    ISimCommandHost::Handler handler;
};

// Registers a list of entries all-or-nothing and unregisters them again. SimPU only.
class SimCommandSet
{
public:
    static const uint32_t kMaxEntries = 64;

    SimCommandSet() = default;
    SimCommandSet(const SimCommandSet&) = delete;
    SimCommandSet& operator=(const SimCommandSet&) = delete;
    ~SimCommandSet() { UnregisterAll(); }

    // Registers entries in order. On a failure at entry n (or a count over kMaxEntries), entries before n are
    // unregistered, ONE Error naming entry n is logged on channel "WorldSim", and false is returned.
    // A set that is already registered is unregistered first. 'owner' names the registrant in the log.
    bool RegisterAll(ISimCommandHost& host, const SimCommandEntry* entries, uint32_t count, const char* owner);

    // Idempotent. Safe to call when nothing (or only a failed attempt) is registered.
    void UnregisterAll();

    uint32_t Count() const { return mCount; }

private:
    struct Registered
    {
        Dia::Core::StringCRC name;
        EntryKind kind = EntryKind::kCommand;
    };

    ISimCommandHost* mHost = nullptr;
    Registered mRegistered[kMaxEntries];
    uint32_t mCount = 0;
};

}  // namespace Debug
}  // namespace CoW
