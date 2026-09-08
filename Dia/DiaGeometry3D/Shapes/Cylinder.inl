namespace Dia::Geometry3D
{
    inline Cylinder::Cylinder()
        : mStartA(0.0f, 0.0f, 0.0f)
        , mEndB(0.0f, 1.0f, 0.0f)
        , mRadius(0.0f)
    {}

    inline Cylinder::Cylinder(const Cylinder& rhs)
        : mStartA(rhs.mStartA)
        , mEndB(rhs.mEndB)
        , mRadius(rhs.mRadius)
    {}

    inline Cylinder::Cylinder(const Dia::Maths::Vector3D& startA,
                               const Dia::Maths::Vector3D& endB,
                               float radius)
        : mStartA(startA)
        , mEndB(endB)
        , mRadius(radius)
    {}

    inline Cylinder& Cylinder::operator=(const Cylinder& rhs)
    {
        mStartA = rhs.mStartA;
        mEndB   = rhs.mEndB;
        mRadius = rhs.mRadius;
        return *this;
    }

    inline bool Cylinder::operator==(const Cylinder& rhs) const
    {
        return mStartA.X() == rhs.mStartA.X() && mStartA.Y() == rhs.mStartA.Y() && mStartA.Z() == rhs.mStartA.Z()
            && mEndB.X() == rhs.mEndB.X() && mEndB.Y() == rhs.mEndB.Y() && mEndB.Z() == rhs.mEndB.Z()
            && mRadius == rhs.mRadius;
    }

    inline bool Cylinder::operator!=(const Cylinder& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Cylinder::GetStartA() const { return mStartA; }
    inline const Dia::Maths::Vector3D& Cylinder::GetEndB()   const { return mEndB; }
    inline float                       Cylinder::GetRadius() const { return mRadius; }

    inline float Cylinder::CalculateLength() const
    {
        return CalculateAxis().Magnitude();
    }

    inline Dia::Maths::Vector3D Cylinder::CalculateAxis() const
    {
        return mEndB - mStartA;
    }

    inline Dia::Maths::Vector3D Cylinder::CalculateAxisDirection() const
    {
        return CalculateAxis().AsNormal();
    }

    inline bool Cylinder::Contains(const Dia::Maths::Vector3D& point) const
    {
        return IsIntersecting(point) != IntersectionClassify::kNoIntersection;
    }
}
