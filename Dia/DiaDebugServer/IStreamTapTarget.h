#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <functional>

namespace Dia { namespace DebugServer {

// Minimal tap interface — owned by DiaDebugServer so DebugServer never
// needs to include DiaApplicationFlow stream headers.
// IStreamStore implements this via the adapter in DiaApplicationFlow.

using TapCallback = std::function<void(const void* bytes, unsigned int size,
                                        const Dia::Core::StringCRC& streamId)>;
struct TapHandle { unsigned int id = 0; };

class IStreamTapTarget
{
public:
    virtual ~IStreamTapTarget() = default;
    virtual TapHandle AttachTap(TapCallback cb) = 0;
    virtual void      DetachTap(TapHandle handle) = 0;
};

}} // namespace Dia::DebugServer
