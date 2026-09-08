namespace Dia { namespace Maths {

//-----------------------------------------------------------------------------
inline Quaternion::Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

//-----------------------------------------------------------------------------
inline Quaternion::Quaternion(float _x, float _y, float _z, float _w)
    : x(_x), y(_y), z(_z), w(_w) {}

//-----------------------------------------------------------------------------
inline Quaternion::Quaternion(const Quaternion& o)
    : x(o.x), y(o.y), z(o.z), w(o.w) {}

//-----------------------------------------------------------------------------
inline Quaternion& Quaternion::operator=(const Quaternion& o)
{
    x = o.x; y = o.y; z = o.z; w = o.w;
    return *this;
}

//-----------------------------------------------------------------------------
inline bool Quaternion::operator==(const Quaternion& r) const
{
    return x == r.x && y == r.y && z == r.z && w == r.w;
}

//-----------------------------------------------------------------------------
inline bool Quaternion::operator!=(const Quaternion& r) const
{
    return !(*this == r);
}

//-----------------------------------------------------------------------------
inline Quaternion Quaternion::Identity()
{
    return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
}

//-----------------------------------------------------------------------------
inline float Quaternion::Dot(const Quaternion& r) const
{
    return x * r.x + y * r.y + z * r.z + w * r.w;
}

//-----------------------------------------------------------------------------
inline float Quaternion::SquareMagnitude() const
{
    return x * x + y * y + z * z + w * w;
}

//-----------------------------------------------------------------------------
inline Quaternion Quaternion::Conjugate() const
{
    return Quaternion(-x, -y, -z, w);
}

//-----------------------------------------------------------------------------
inline Quaternion Quaternion::AsNormal() const
{
    Quaternion q(*this);
    q.Normalize();
    return q;
}

} }
