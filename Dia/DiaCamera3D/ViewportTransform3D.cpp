#include "DiaCamera3D/ViewportTransform3D.h"

#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Vector/Vector4D.h"

namespace Dia::Camera3D
{
    ViewportTransform3D::ViewportTransform3D(const Camera3D& camera, Dia::Maths::Vector2D windowSize)
        : mWindowSize(windowSize)
    {
        // Build view matrix: LookAt(eye, target, up)
        const Dia::Maths::Vector3D forward   = camera.orientation.Rotate(Dia::Maths::Vector3D(0.0f, 0.0f, -1.0f));
        const Dia::Maths::Vector3D up        = camera.orientation.Rotate(Dia::Maths::Vector3D(0.0f, 1.0f,  0.0f));
        const Dia::Maths::Vector3D target    = camera.position + forward;

        mViewMatrix = Dia::Maths::Matrix44::LookAt(camera.position, target, up);

        // Build projection matrix
        if (camera.projectionType == ProjectionType::Perspective)
        {
            const float aspect = windowSize.x / windowSize.y;
            mProjectionMatrix = Dia::Maths::Matrix44::Perspective(
                Dia::Maths::Angle::FromDegrees(camera.perspective.fovY),
                aspect,
                camera.perspective.nearZ,
                camera.perspective.farZ);
        }
        else
        {
            const float halfW = camera.ortho.width  * 0.5f;
            const float halfH = camera.ortho.height * 0.5f;
            mProjectionMatrix = Dia::Maths::Matrix44::Orthographic(
                -halfW,  halfW,
                -halfH,  halfH,
                camera.ortho.nearZ,
                camera.ortho.farZ);
        }
    }

    bool ViewportTransform3D::WorldToScreen(Dia::Maths::Vector3D world,
                                            Dia::Maths::Vector2D& outScreen) const
    {
        // Transform to clip space: projection * view * worldPoint
        const Dia::Maths::Matrix44 viewProj = mProjectionMatrix * mViewMatrix;
        const Dia::Maths::Vector4D clip = viewProj.TransformVector4(
            Dia::Maths::Vector4D(world.x, world.y, world.z, 1.0f));

        if (clip.w <= 0.0f)
        {
            return false;
        }

        // Perspective divide → NDC
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;

        // NDC → screen pixels (Y-flip: NDC +1 is top, screen 0 is top)
        outScreen.x = (ndcX * 0.5f + 0.5f) * mWindowSize.x;
        outScreen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * mWindowSize.y;

        return true;
    }

    void ViewportTransform3D::ScreenToWorldRay(Dia::Maths::Vector2D screen,
                                               Dia::Maths::Vector3D& outOrigin,
                                               Dia::Maths::Vector3D& outDirection) const
    {
        // Screen → NDC (Y-flip: screen top = NDC +1)
        const float ndcX =  (screen.x / mWindowSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screen.y / mWindowSize.y) * 2.0f;

        // Inverse view-projection matrix
        const Dia::Maths::Matrix44 viewProj    = mProjectionMatrix * mViewMatrix;
        const Dia::Maths::Matrix44 invViewProj = viewProj.Inverse();

        // Unproject near point (NDC z = -1)
        const Dia::Maths::Vector4D nearClip = invViewProj.TransformVector4(
            Dia::Maths::Vector4D(ndcX, ndcY, -1.0f, 1.0f));
        const Dia::Maths::Vector3D nearWorld(
            nearClip.x / nearClip.w,
            nearClip.y / nearClip.w,
            nearClip.z / nearClip.w);

        // Unproject far point (NDC z = +1)
        const Dia::Maths::Vector4D farClip = invViewProj.TransformVector4(
            Dia::Maths::Vector4D(ndcX, ndcY, 1.0f, 1.0f));
        const Dia::Maths::Vector3D farWorld(
            farClip.x / farClip.w,
            farClip.y / farClip.w,
            farClip.z / farClip.w);

        outOrigin    = nearWorld;
        outDirection = (farWorld - nearWorld).AsNormal();
    }

    Dia::Geometry3D::Frustum ViewportTransform3D::ExtractFrustum() const
    {
        return Dia::Geometry3D::Frustum::FromMatrix44(mProjectionMatrix * mViewMatrix);
    }

    const Dia::Maths::Matrix44& ViewportTransform3D::GetViewMatrix() const
    {
        return mViewMatrix;
    }

    const Dia::Maths::Matrix44& ViewportTransform3D::GetProjectionMatrix() const
    {
        return mProjectionMatrix;
    }
}
