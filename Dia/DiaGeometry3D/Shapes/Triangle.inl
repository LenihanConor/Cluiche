namespace Dia::Geometry3D
{
    inline Triangle::Triangle()
        : mV0(0.0f, 0.0f, 0.0f)
        , mV1(1.0f, 0.0f, 0.0f)
        , mV2(0.0f, 0.0f, 1.0f)
    {}

    inline Triangle::Triangle(const Triangle& rhs)
        : mV0(rhs.mV0)
        , mV1(rhs.mV1)
        , mV2(rhs.mV2)
    {}

    inline Triangle::Triangle(const Dia::Maths::Vector3D& v0,
                               const Dia::Maths::Vector3D& v1,
                               const Dia::Maths::Vector3D& v2)
        : mV0(v0)
        , mV1(v1)
        , mV2(v2)
    {}

    inline Triangle& Triangle::operator=(const Triangle& rhs)
    {
        mV0 = rhs.mV0;
        mV1 = rhs.mV1;
        mV2 = rhs.mV2;
        return *this;
    }

    inline bool Triangle::operator==(const Triangle& rhs) const
    {
        return mV0.X() == rhs.mV0.X() && mV0.Y() == rhs.mV0.Y() && mV0.Z() == rhs.mV0.Z()
            && mV1.X() == rhs.mV1.X() && mV1.Y() == rhs.mV1.Y() && mV1.Z() == rhs.mV1.Z()
            && mV2.X() == rhs.mV2.X() && mV2.Y() == rhs.mV2.Y() && mV2.Z() == rhs.mV2.Z();
    }

    inline bool Triangle::operator!=(const Triangle& rhs) const { return !(*this == rhs); }

    inline const Dia::Maths::Vector3D& Triangle::GetV0() const { return mV0; }
    inline const Dia::Maths::Vector3D& Triangle::GetV1() const { return mV1; }
    inline const Dia::Maths::Vector3D& Triangle::GetV2() const { return mV2; }

    inline Dia::Maths::Vector3D Triangle::CalculateNormal() const
    {
        return (mV1 - mV0).Cross(mV2 - mV0).AsNormal();
    }

    inline float Triangle::CalculateArea() const
    {
        return (mV1 - mV0).Cross(mV2 - mV0).Magnitude() * 0.5f;
    }

    inline Dia::Maths::Vector3D Triangle::CalculateCentroid() const
    {
        return (mV0 + mV1 + mV2) * (1.0f / 3.0f);
    }
}
