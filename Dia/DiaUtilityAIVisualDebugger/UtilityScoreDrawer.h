////////////////////////////////////////////////////////////////////////////////
// Filename: UtilityScoreDrawer.h
// Description: IVisualDebugger that renders a per-action score table via ImGui.
//              Draw() is a no-op — utility scores have no world-space anchor.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::UtilityAI { class UtilitySet; }

namespace Dia::UtilityAI
{

class UtilityScoreDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit UtilityScoreDrawer(const UtilitySet& utilitySet);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const UtilitySet& mUtilitySet;
};

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
