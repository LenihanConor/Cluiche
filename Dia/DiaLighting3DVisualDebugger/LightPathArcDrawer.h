////////////////////////////////////////////////////////////////////////////////
// Filename: LightPathArcDrawer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D { class LightRegistry3D;  } }
namespace Dia { namespace Debug      { class DebugLayerManager; } }
namespace Dia { namespace Observation { namespace Metric { class Counter; } } }

namespace Dia { namespace Lighting3D {

class LightPathArcDrawer : public Dia::Debug::IVisualDebugger
{
public:
    LightPathArcDrawer(const LightRegistry3D&               registry,
                       const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw    (Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

    void SetArcSamples(int samples) { mArcSamples = samples; }

private:
    const LightRegistry3D&               mRegistry;
    const Dia::Debug::DebugLayerManager& mManager;

    int                                          mArcSamples    = 32;
    Dia::Observation::Metric::Counter*           mActivePathsCounter = nullptr;
};

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
