////////////////////////////////////////////////////////////////////////////////
// Filename: MeshOriginDrawer.h
// Description: IVisualDebugger that draws a 3-axis cross at the origin of each
//              mesh draw command submitted in the current frame.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }
namespace Dia { namespace Debug { class DebugLayerManager; } }

namespace Dia { namespace Mesh3D {

class MeshOriginDrawer : public Dia::Debug::IVisualDebugger
{
public:
    MeshOriginDrawer(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                     const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Debug::DebugLayerManager&    mManager;
    bool                                    mHighlightSkinned = false;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
