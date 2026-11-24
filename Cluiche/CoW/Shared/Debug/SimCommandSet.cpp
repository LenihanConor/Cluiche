#include <Shared/Debug/SimCommandSet.h>

#include <DiaObservation/Log/DiaLog.h>

namespace CoW {
namespace Debug {

bool SimCommandSet::RegisterAll(ISimCommandHost& host, const SimCommandEntry* entries, uint32_t count, const char* owner)
{
    UnregisterAll();
    const char* ownerName = owner != nullptr ? owner : "";
    if (count > kMaxEntries)
    {
        DIA_LOG_ERROR("WorldSim", "%s: %u commands exceeds the set limit of %u, none registered", ownerName,
                      static_cast<unsigned>(count), static_cast<unsigned>(kMaxEntries));
        return false;
    }
    mHost = &host;
    for (uint32_t i = 0; i < count; ++i)
    {
        const SimCommandEntry& e = entries[i];
        const Dia::Core::StringCRC name(e.name != nullptr ? e.name : "");
        const bool ok = e.name != nullptr
            && (e.kind == EntryKind::kCommand ? host.RegisterCommand(name, e.help, e.handler)
                                              : host.RegisterQuery(name, e.help, e.handler));
        if (!ok)
        {
            DIA_LOG_ERROR("WorldSim", "%s: registration of %s '%s' (entry %u of %u) was rejected, rolling back", ownerName,
                          e.kind == EntryKind::kCommand ? "command" : "query", e.name != nullptr ? e.name : "(null)",
                          static_cast<unsigned>(i + 1), static_cast<unsigned>(count));
            UnregisterAll();
            return false;
        }
        mRegistered[mCount].name = name;
        mRegistered[mCount].kind = e.kind;
        ++mCount;
    }
    return true;
}

void SimCommandSet::UnregisterAll()
{
    if (mHost != nullptr)
    {
        // reverse order of registration
        for (uint32_t i = mCount; i > 0; --i)
        {
            if (!mHost->Unregister(mRegistered[i - 1].name, mRegistered[i - 1].kind))
            {
                DIA_LOG_WARNING("WorldSim", "unregister of '%s' failed", mRegistered[i - 1].name.AsChar());
            }
        }
    }
    mCount = 0;
    mHost = nullptr;
}

}  // namespace Debug
}  // namespace CoW
