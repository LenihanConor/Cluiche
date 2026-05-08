#pragma once

namespace Dia { namespace Animation2D {
    struct SpringNodeDef {
        float stiffness          = 50.0f;
        float damping            = 5.0f;
        float maxAngularVelocity = 20.0f;
    };

    // Aliases for ergonomic use
    using SpringNodeParams = SpringNodeDef;
    using SpringParams     = SpringNodeDef;
} }
