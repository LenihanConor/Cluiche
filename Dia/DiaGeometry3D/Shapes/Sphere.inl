namespace Dia::Geometry3D
{
    inline Sphere::Sphere()
        : mCenter(0.0f, 0.0f, 0.0f)
        , mRadius(0.0f)
    {}

    inline Sphere::Sphere(const Sphere& rhs)
        : mCenter(rhs.mCenter)
        , mRadius(rhs.mRadius)
    {}

    inline Sphere::Sphere(const Dia::Maths::Vector3D& center, float radius)
        : mCenter(center)
        , mRadius(radius)
    {}

    inline Sphere& Sphere::operator=(const Sphere& rhs)
    {
        mCenter = rhs.mCenter;
        mRadius = rhs.mRadius;
        return *this;
    }

    inline bool Sphere::operator==(const Sphere& rhs) const
    {
        return mCenter.X() == rhs.mCenter.X() && mCenter.Y() == rhs.mCenter.Y() && mCenter.Z() == rhs.mCenter.Z()
            && mRadius == rhs.mRadius;
    }

    inline bool Sphere::operator!=(const Sphere& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Sphere::GetCenter() const { return mCenter; }
    inline float                       Sphere::GetRadius() const { return mRadius; }

    inline float Sphere::CalculateVolume() const
    {
        return (4.0f / 3.0f) * 3.14159265358979323846f * mRadius * mRadius * mRadius;
    }

    inline float Sphere::CalculateSurfaceArea() const
    {
        return 4.0f * 3.14159265358979323846f * mRadius * mRadius;
    }

    inline bool Sphere::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
