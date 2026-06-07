#include "DiaGraphics3D/Camera3D.h"

namespace Dia { namespace Graphics3D {

Camera3D::Camera3D()
    : view(Dia::Maths::Matrix44::Identity())
    , projection(Dia::Maths::Matrix44::Identity())
{
}

void Camera3D::SetPerspective(const Dia::Maths::Angle& fovY, float aspect, float nearZ, float farZ)
{
    projection = Dia::Maths::Matrix44::Perspective(fovY, aspect, nearZ, farZ);
}

void Camera3D::SetOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    projection = Dia::Maths::Matrix44::Orthographic(left, right, bottom, top, nearZ, farZ);
}

void Camera3D::SetView(const Dia::Maths::Vector3D& eye,
                       const Dia::Maths::Vector3D& target,
                       const Dia::Maths::Vector3D& up)
{
    view = Dia::Maths::Matrix44::LookAt(eye, target, up);
}

} }
