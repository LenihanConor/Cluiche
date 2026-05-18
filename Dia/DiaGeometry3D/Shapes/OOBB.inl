namespace Dia::Geometry3D
{
    inline OOBB::OOBB()
        : mCenter(0.0f, 0.0f, 0.0f)
        , mHalfExtents(0.0f, 0.0f, 0.0f)
        , mOrientation(Dia::Maths::Quaternion::Identity())
    {}

    inline OOBB::OOBB(const OOBB& rhs)
        : mCenter(rhs.mCenter)
        , mHalfExtents(rhs.mHalfExtents)
        , mOrientation(rhs.mOrientation)
    {}

    inline OOBB::OOBB(const Dia::Maths::Vector3D& center,
                      const Dia::Maths::Vector3D& halfExtents,
                      const Dia::Maths::Quaternion& orientation)
        : mCenter(center)
        , mHalfExtents(halfExtents)
        , mOrientation(orientation)
    {}

    inline OOBB& OOBB::operator=(const OOBB& rhs)
    {
        mCenter      = rhs.mCenter;
        mHalfExtents = rhs.mHalfExtents;
        mOrientation = rhs.mOrientation;
        return *this;
    }

    inline bool OOBB::operator==(const OOBB& rhs) const
    {
        return mCenter.X() == rhs.mCenter.X() && mCenter.Y() == rhs.mCenter.Y() && mCenter.Z() == rhs.mCenter.Z()
            && mHalfExtents.X() == rhs.mHalfExtents.X() && mHalfExtents.Y() == rhs.mHalfExtents.Y() && mHalfExtents.Z() == rhs.mHalfExtents.Z()
            && mOrientation == rhs.mOrientation;
    }

    inline bool OOBB::operator!=(const OOBB& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D&   OOBB::GetCenter()      const { return mCenter; }
    inline const Dia::Maths::Vector3D&   OOBB::GetHalfExtents() const { return mHalfExtents; }
    inline const Dia::Maths::Quaternion& OOBB::GetOrientation() const { return mOrientation; }

    inline void OOBB::GetAxes(Dia::Maths::Vector3D& outX,
                               Dia::Maths::Vector3D& outY,
                               Dia::Maths::Vector3D& outZ) const
    {
        outX = mOrientation.Rotate(Dia::Maths::Vector3D::XAxis());
        outY = mOrientation.Rotate(Dia::Maths::Vector3D::YAxis());
        outZ = mOrientation.Rotate(Dia::Maths::Vector3D::ZAxis());
    }

    inline bool OOBB::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
