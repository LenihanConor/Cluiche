#include "DiaGraphics3D/Camera3D.h"

#include <cmath>

namespace Dia { namespace Graphics3D {

Camera3D::Camera3D()
    : view(Dia::Maths::Matrix44::Identity())
    , projection(Dia::Maths::Matrix44::Identity())
{
}

void Camera3D::SetPerspective(const Dia::Maths::Angle& fovY, float aspect, float nearZ, float farZ)
{
    projection = Dia::Maths::Matrix44::Perspective(fovY, aspect, nearZ, farZ);
    fovYDeg    = fovY.AsDegrees();
    this->nearZ = nearZ;
    this->farZ  = farZ;
}

void Camera3D::SetOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    projection = Dia::Maths::Matrix44::Orthographic(left, right, bottom, top, nearZ, farZ);
}

void Camera3D::SetView(const Dia::Maths::Vector3D& eye,
                       const Dia::Maths::Vector3D& target,
                       const Dia::Maths::Vector3D& up)
{
    view         = Dia::Maths::Matrix44::LookAt(eye, target, up);
    this->eye    = eye;
    const float dx = target.x - eye.x;
    const float dy = target.y - eye.y;
    const float dz = target.z - eye.z;
    const float mag = sqrtf(dx * dx + dy * dy + dz * dz);
    forward = (mag > 1e-6f)
        ? Dia::Maths::Vector3D(dx / mag, dy / mag, dz / mag)
        : Dia::Maths::Vector3D(0.0f, 0.0f, -1.0f);
}

} }
