namespace Dia::Geometry3D
{
    inline Frustum::Frustum()
    {
        // Default: unit cube frustum — planes at ±1 on each axis, inward normals
        mPlanes[static_cast<int>(FrustumPlane::kNear)]   = Plane(Dia::Maths::Vector3D( 0.0f,  0.0f,  1.0f),  -1.0f);
        mPlanes[static_cast<int>(FrustumPlane::kFar)]    = Plane(Dia::Maths::Vector3D( 0.0f,  0.0f, -1.0f),  -1.0f);
        mPlanes[static_cast<int>(FrustumPlane::kLeft)]   = Plane(Dia::Maths::Vector3D( 1.0f,  0.0f,  0.0f),  -1.0f);
        mPlanes[static_cast<int>(FrustumPlane::kRight)]  = Plane(Dia::Maths::Vector3D(-1.0f,  0.0f,  0.0f),  -1.0f);
        mPlanes[static_cast<int>(FrustumPlane::kTop)]    = Plane(Dia::Maths::Vector3D( 0.0f, -1.0f,  0.0f),  -1.0f);
        mPlanes[static_cast<int>(FrustumPlane::kBottom)] = Plane(Dia::Maths::Vector3D( 0.0f,  1.0f,  0.0f),  -1.0f);
    }

    inline Frustum::Frustum(const Frustum& rhs)
    {
        for (int i = 0; i < 6; ++i)
            mPlanes[i] = rhs.mPlanes[i];
    }

    inline Frustum::Frustum(const Plane& near, const Plane& far,
                             const Plane& left, const Plane& right,
                             const Plane& top,  const Plane& bottom)
    {
        mPlanes[static_cast<int>(FrustumPlane::kNear)]   = near;
        mPlanes[static_cast<int>(FrustumPlane::kFar)]    = far;
        mPlanes[static_cast<int>(FrustumPlane::kLeft)]   = left;
        mPlanes[static_cast<int>(FrustumPlane::kRight)]  = right;
        mPlanes[static_cast<int>(FrustumPlane::kTop)]    = top;
        mPlanes[static_cast<int>(FrustumPlane::kBottom)] = bottom;
    }

    inline Frustum& Frustum::operator=(const Frustum& rhs)
    {
        for (int i = 0; i < 6; ++i)
            mPlanes[i] = rhs.mPlanes[i];
        return *this;
    }

    inline bool Frustum::operator==(const Frustum& rhs) const
    {
        for (int i = 0; i < 6; ++i)
            if (mPlanes[i] != rhs.mPlanes[i]) return false;
        return true;
    }

    inline bool Frustum::operator!=(const Frustum& rhs) const { return !(*this == rhs); }

    inline const Plane& Frustum::GetPlane(FrustumPlane slot) const
    {
        return mPlanes[static_cast<int>(slot)];
    }

    inline bool Frustum::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
