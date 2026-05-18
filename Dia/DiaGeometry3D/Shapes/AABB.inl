namespace Dia::Geometry3D
{
    inline AABB::AABB()
        : mMin(0.0f, 0.0f, 0.0f)
        , mMax(0.0f, 0.0f, 0.0f)
    {}

    inline AABB::AABB(const AABB& rhs)
        : mMin(rhs.mMin)
        , mMax(rhs.mMax)
    {}

    inline AABB::AABB(const Dia::Maths::Vector3D& min, const Dia::Maths::Vector3D& max)
        : mMin(min)
        , mMax(max)
    {}

    inline AABB& AABB::operator=(const AABB& rhs)
    {
        mMin = rhs.mMin;
        mMax = rhs.mMax;
        return *this;
    }

    inline bool AABB::operator==(const AABB& rhs) const
    {
        return mMin.X() == rhs.mMin.X() && mMin.Y() == rhs.mMin.Y() && mMin.Z() == rhs.mMin.Z()
            && mMax.X() == rhs.mMax.X() && mMax.Y() == rhs.mMax.Y() && mMax.Z() == rhs.mMax.Z();
    }

    inline bool AABB::operator!=(const AABB& rhs) const
    {
        return !(*this == rhs);
    }

    inline AABB AABB::FromCenterExtents(const Dia::Maths::Vector3D& center, const Dia::Maths::Vector3D& extents)
    {
        return AABB(center - extents, center + extents);
    }

    inline const Dia::Maths::Vector3D& AABB::GetMin() const { return mMin; }
    inline const Dia::Maths::Vector3D& AABB::GetMax() const { return mMax; }

    inline Dia::Maths::Vector3D AABB::CalculateCenter() const
    {
        return (mMin + mMax) * 0.5f;
    }

    inline Dia::Maths::Vector3D AABB::CalculateExtents() const
    {
        return (mMax - mMin) * 0.5f;
    }

    inline float AABB::CalculateSurfaceArea() const
    {
        Dia::Maths::Vector3D size = mMax - mMin;
        return 2.0f * (size.X() * size.Y() + size.Y() * size.Z() + size.Z() * size.X());
    }

    inline float AABB::CalculateVolume() const
    {
        Dia::Maths::Vector3D size = mMax - mMin;
        return size.X() * size.Y() * size.Z();
    }

    inline void AABB::Encapsulate(const Dia::Maths::Vector3D& point)
    {
        if (point.X() < mMin.X()) mMin.X(point.X());
        if (point.Y() < mMin.Y()) mMin.Y(point.Y());
        if (point.Z() < mMin.Z()) mMin.Z(point.Z());
        if (point.X() > mMax.X()) mMax.X(point.X());
        if (point.Y() > mMax.Y()) mMax.Y(point.Y());
        if (point.Z() > mMax.Z()) mMax.Z(point.Z());
    }

    inline void AABB::Encapsulate(const AABB& other)
    {
        Encapsulate(other.mMin);
        Encapsulate(other.mMax);
    }

    inline bool AABB::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
