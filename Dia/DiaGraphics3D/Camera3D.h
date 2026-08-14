#pragma once
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Core/Angle.h>

namespace Dia { namespace Graphics3D {

struct Camera3D
{
    Camera3D();

    Dia::Maths::Matrix44 view;        // world -> view (row-major per DiaMaths SD-005)
    Dia::Maths::Matrix44 projection;  // view -> clip

    // Cached from last SetView / SetPerspective call (zero-initialised until set).
    Dia::Maths::Vector3D eye;
    Dia::Maths::Vector3D forward;
    float fovYDeg = 0.0f;
    float nearZ   = 0.0f;
    float farZ    = 0.0f;

    void SetPerspective(const Dia::Maths::Angle& fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
    void SetView(const Dia::Maths::Vector3D& eye,
                 const Dia::Maths::Vector3D& target,
                 const Dia::Maths::Vector3D& up);
};

} }
