#include "DiaGraphics3D/Mesh3DDrawCommand.h"

namespace Dia { namespace Graphics3D {

Mesh3DDrawCommand::Mesh3DDrawCommand()
    : meshId()
    , materialId()
    , transform(Dia::Maths::Matrix44::Identity())
    , skinningPaletteIndex(0)
    , layer(0)
{
}

} }
