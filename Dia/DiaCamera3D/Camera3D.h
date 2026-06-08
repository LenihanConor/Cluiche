#pragma once

#include "DiaMaths/Vector/Vector3D.h"
#include "DiaMaths/Quaternion/Quaternion.h"

#include <cstdint>

namespace Dia::Camera3D
{
    struct PerspectiveParams
    {
        float fovY  = 60.0f;   // degrees
        float nearZ = 0.1f;
        float farZ  = 1000.0f;
    };

    struct OrthoParams
    {
        float width  = 10.0f;
        float height = 10.0f;
        float nearZ  = -100.0f;
        float farZ   =  100.0f;
    };

    enum class ProjectionType : uint8_t { Perspective, Orthographic };

    struct Camera3D
    {
        Dia::Maths::Vector3D   position;
        Dia::Maths::Quaternion orientation;   // identity = looking down -Z, up is +Y
        ProjectionType         projectionType = ProjectionType::Perspective;
        PerspectiveParams      perspective;
        OrthoParams            ortho;
    };
}
