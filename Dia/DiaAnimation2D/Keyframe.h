#pragma once
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Animation2D {
    struct Keyframe {
        float time     = 0.0f;
        float rotation = 0.0f;
        Dia::Maths::Vector2D position = Dia::Maths::Vector2D(0.0f, 0.0f);
        Dia::Maths::Vector2D scale    = Dia::Maths::Vector2D(1.0f, 1.0f);
    };
} }
