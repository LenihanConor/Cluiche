////////////////////////////////////////////////////////////////////////////////
// Filename: FrameData3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaGraphics/Frame/FrameData.h>
#include "DiaGraphics3D/Mesh3DFrameData.h"

namespace Dia { namespace Graphics3D {

///
/// FrameData3D - Combined 2D+3D frame packet.
/// Inherits the full 2D/UI/debug payload from Dia::Graphics::FrameData
/// and the 3D mesh/light payload from Mesh3DFrameData.
///
class FrameData3D
    : public Dia::Graphics::FrameData
    , public Mesh3DFrameData
{
public:
    FrameData3D();

    FrameData3D& operator=(const FrameData3D& rhs);

    void Clear();
    void Copy(const FrameData3D& rhs);
};

} }
