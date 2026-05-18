namespace Dia::Geometry3D
{
    inline Capsule::Capsule()
        : mStartA(0.0f, 0.0f, 0.0f)
        , mEndB(0.0f, 1.0f, 0.0f)
        , mRadius(0.0f)
    {}

    inline Capsule::Capsule(const Capsule& rhs)
        : mStartA(rhs.mStartA)
        , mEndB(rhs.mEndB)
        , mRadius(rhs.mRadius)
    {}

    inline Capsule::Capsule(const Dia::Maths::Vector3D& startA,
                             const Dia::Maths::Vector3D& endB,
                             float radius)
        : mStartA(startA)
        , mEndB(endB)
        , mRadius(radius)
    {}

    inline Capsule& Capsule::operator=(const Capsule& rhs)
    {
        mStartA = rhs.mStartA;
        mEndB   = rhs.mEndB;
        mRadius = rhs.mRadius;
        return *this;
    }

    inline bool Capsule::operator==(const Capsule& rhs) const
    {
        return mStartA.X() == rhs.mStartA.X() && mStartA.Y() == rhs.mStartA.Y() && mStartA.Z() == rhs.mStartA.Z()
            && mEndB.X() == rhs.mEndB.X() && mEndB.Y() == rhs.mEndB.Y() && mEndB.Z() == rhs.mEndB.Z()
            && mRadius == rhs.mRadius;
    }

    inline bool Capsule::operator!=(const Capsule& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Capsule::GetStartA() const { return mStartA; }
    inline const Dia::Maths::Vector3D& Capsule::GetEndB()   const { return mEndB; }
    inline float                       Capsule::GetRadius() const { return mRadius; }

    inline float Capsule::CalculateLength() const
    {
        return CalculateAxis().Magnitude();
    }

    inline Dia::Maths::Vector3D Capsule::CalculateAxis() const
    {
        return mEndB - mStartA;
    }

    inline Dia::Maths::Vector3D Capsule::CalculateAxisDirection() const
    {
        return CalculateAxis().AsNormal();
    }

    inline bool Capsule::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
