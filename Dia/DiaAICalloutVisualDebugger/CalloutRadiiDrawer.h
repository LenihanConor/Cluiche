#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::AICallout { class CalloutRegistry; }

namespace Dia::AICalloutVisualDebugger {

class CalloutRadiiDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static const Dia::Core::StringCRC kLayerName;

    explicit CalloutRadiiDrawer(const Dia::AICallout::CalloutRegistry& registry);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::AICallout::CalloutRegistry& mRegistry;
};

} // namespace Dia::AICalloutVisualDebugger

#endif // DIA_DEBUG
