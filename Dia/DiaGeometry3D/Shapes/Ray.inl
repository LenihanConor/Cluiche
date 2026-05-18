namespace Dia::Geometry3D
{
    inline Ray::Ray()
        : mOrigin(0.0f, 0.0f, 0.0f)
        , mDirection(0.0f, 0.0f, 1.0f)
    {}

    inline Ray::Ray(const Ray& rhs)
        : mOrigin(rhs.mOrigin)
        , mDirection(rhs.mDirection)
    {}

    inline Ray& Ray::operator=(const Ray& rhs)
    {
        mOrigin    = rhs.mOrigin;
        mDirection = rhs.mDirection;
        return *this;
    }

    inline bool Ray::operator==(const Ray& rhs) const
    {
        return mOrigin.X() == rhs.mOrigin.X() && mOrigin.Y() == rhs.mOrigin.Y() && mOrigin.Z() == rhs.mOrigin.Z()
            && mDirection.X() == rhs.mDirection.X() && mDirection.Y() == rhs.mDirection.Y() && mDirection.Z() == rhs.mDirection.Z();
    }

    inline bool Ray::operator!=(const Ray& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Ray::GetOrigin()    const { return mOrigin; }
    inline const Dia::Maths::Vector3D& Ray::GetDirection() const { return mDirection; }

    inline Dia::Maths::Vector3D Ray::GetPointAt(float t) const
    {
        return mOrigin + mDirection * t;
    }
}
