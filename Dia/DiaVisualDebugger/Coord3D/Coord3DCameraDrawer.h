////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DCameraDrawer.h
// Description: IVisualDebugger that exposes camera state as ImGui text (no geometry).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Debug
{

class Coord3DCameraDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit Coord3DCameraDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
