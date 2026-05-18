namespace Dia::Geometry3D
{
    inline Plane::Plane()
        : mNormal(0.0f, 1.0f, 0.0f)
        , mD(0.0f)
    {}

    inline Plane::Plane(const Plane& rhs)
        : mNormal(rhs.mNormal)
        , mD(rhs.mD)
    {}

    inline Plane::Plane(const Dia::Maths::Vector3D& normal, float d)
        : mNormal(normal)
        , mD(d)
    {}

    inline Plane& Plane::operator=(const Plane& rhs)
    {
        mNormal = rhs.mNormal;
        mD      = rhs.mD;
        return *this;
    }

    inline bool Plane::operator==(const Plane& rhs) const
    {
        return mNormal.X() == rhs.mNormal.X() && mNormal.Y() == rhs.mNormal.Y() && mNormal.Z() == rhs.mNormal.Z()
            && mD == rhs.mD;
    }

    inline bool Plane::operator!=(const Plane& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Plane::GetNormal() const { return mNormal; }
    inline float                       Plane::GetD()      const { return mD; }

    inline float Plane::DistanceTo(const Dia::Maths::Vector3D& point) const
    {
        return mNormal.Dot(point) - mD;
    }

    inline PlaneSide Plane::ClassifyPoint(const Dia::Maths::Vector3D& point, float epsilon) const
    {
        const float dist = DistanceTo(point);
        if (dist >  epsilon) return PlaneSide::kFront;
        if (dist < -epsilon) return PlaneSide::kBehind;
        return PlaneSide::kOnPlane;
    }
}
