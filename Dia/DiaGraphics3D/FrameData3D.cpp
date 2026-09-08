////////////////////////////////////////////////////////////////////////////////
// Filename: FrameData3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics3D/FrameData3D.h"

namespace Dia { namespace Graphics3D {

FrameData3D::FrameData3D()
    : Dia::Graphics::FrameData()
    , Mesh3DFrameData()
{}

FrameData3D& FrameData3D::operator=(const FrameData3D& rhs)
{
    Dia::Graphics::FrameData::operator=(rhs);
    Mesh3DFrameData::Copy(rhs);
    return *this;
}

void FrameData3D::Clear()
{
    Dia::Graphics::FrameData::Clear();
    Mesh3DFrameData::Clear();
}

void FrameData3D::Copy(const FrameData3D& rhs)
{
    Dia::Graphics::FrameData::Copy(rhs);
    Mesh3DFrameData::Copy(rhs);
}

} }
