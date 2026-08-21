#pragma once
#include <cstdint>

namespace Dia::MessageBus {

    // Pass tag for Subscribe — controls which flush pass the handler runs in.
    // Bus::Update() runs a Primary sweep followed by a Reaction sweep each
    // tick. Primary-pass handlers may Post/Broadcast; Reaction-pass handlers
    // may not (Bus::Post is guarded while the Reaction sweep is executing).
    enum class Pass : uint8_t { Primary, Reaction };

} // namespace Dia::MessageBus
