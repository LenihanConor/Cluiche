#pragma once
#include <cstdint>

namespace Dia::MessageBus {

    // Pass tag for Subscribe — controls which flush pass the handler runs in.
    // NOTE: for this task only Pass::Primary is dispatched. Pass::Reaction is
    // accepted by Subscribe() and stored, but no dispatch path fires for it yet
    // (Reaction sweep is a separate, later task).
    enum class Pass : uint8_t { Primary, Reaction };

} // namespace Dia::MessageBus
