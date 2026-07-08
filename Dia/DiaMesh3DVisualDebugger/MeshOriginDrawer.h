////////////////////////////////////////////////////////////////////////////////
// Filename: MeshOriginDrawer.h
// Description: IVisualDebugger that draws a 3-axis cross at the origin of each
//              mesh draw command submitted in the current frame.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Core { class IDebugContext; } }

namespace Dia { namespace Mesh3D {

class MeshOriginDrawer : public Dia::Debug::IVisualDebugger
{
public:
    MeshOriginDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                     const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Core::IDebugContext&         mManager;
    bool                                    mHighlightSkinned = false;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
