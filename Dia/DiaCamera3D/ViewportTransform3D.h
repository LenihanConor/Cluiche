#pragma once

#include "DiaCamera3D/Camera3D.h"
#include "DiaMaths/Matrix/Matrix44.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometry3D/Shapes/Frustum.h"

namespace Dia::Camera3D
{
    class ViewportTransform3D
    {
    public:
        // windowSize drives aspect ratio for Perspective cameras.
        ViewportTransform3D(const Camera3D& camera, Dia::Maths::Vector2D windowSize);

        // Project world point to screen pixel. Returns false if behind the camera (w <= 0).
        bool WorldToScreen(Dia::Maths::Vector3D world,
                           Dia::Maths::Vector2D& outScreen) const;

        // Unproject screen pixel to world-space ray (normalised direction).
        void ScreenToWorldRay(Dia::Maths::Vector2D screen,
                              Dia::Maths::Vector3D& outOrigin,
                              Dia::Maths::Vector3D& outDirection) const;

        // Extract view frustum (useful for culling callers — e.g. DiaScene3D).
        Dia::Geometry3D::Frustum ExtractFrustum() const;

        const Dia::Maths::Matrix44& GetViewMatrix()       const;
        const Dia::Maths::Matrix44& GetProjectionMatrix() const;

    private:
        Dia::Maths::Matrix44 mViewMatrix;
        Dia::Maths::Matrix44 mProjectionMatrix;
        Dia::Maths::Vector2D mWindowSize;
    };
}
