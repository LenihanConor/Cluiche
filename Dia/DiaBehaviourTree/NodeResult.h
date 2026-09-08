#pragma once

namespace Dia { namespace BehaviourTree {
    enum class NodeResult {
        kRunning,   // node is still executing — resume next tick
        kSuccess,   // node completed successfully
        kFailure    // node failed — parent handles propagation
    };
} }
