#pragma once

#include <stdint.h>

#include <Shared/Debug/SimCommandSet.h>

namespace CoW {
namespace Debug {
namespace Testing {

// Fake ISimCommandHost: records registrations and unregistrations; registration attempt number failOnNth (1-based) fails.
struct FakeSimCommandHost : public ISimCommandHost
{
    static const uint32_t kMax = 64;

    bool RegisterQuery(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo&, Handler) override
    {
        return Record(name, EntryKind::kQuery);
    }
    bool RegisterCommand(const Dia::Core::StringCRC& name, const Dia::DebugServer::SimHelpInfo&, Handler) override
    {
        return Record(name, EntryKind::kCommand);
    }
    bool Unregister(const Dia::Core::StringCRC& name, EntryKind kind) override
    {
        if (unregCount < kMax) { unregNames[unregCount] = name; unregKinds[unregCount] = kind; ++unregCount; }
        return true;
    }

    uint32_t failOnNth = 0;
    uint32_t attempts = 0;
    uint32_t regCount = 0;
    Dia::Core::StringCRC regNames[kMax];
    EntryKind regKinds[kMax] = {};
    uint32_t unregCount = 0;
    Dia::Core::StringCRC unregNames[kMax];
    EntryKind unregKinds[kMax] = {};

private:
    bool Record(const Dia::Core::StringCRC& name, EntryKind kind)
    {
        ++attempts;
        if (attempts == failOnNth) { return false; }
        if (regCount < kMax) { regNames[regCount] = name; regKinds[regCount] = kind; ++regCount; }
        return true;
    }
};

}  // namespace Testing
}  // namespace Debug
}  // namespace CoW
