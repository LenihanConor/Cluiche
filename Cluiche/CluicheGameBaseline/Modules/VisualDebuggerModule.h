#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaVisualDebugger/DebugLayerManager.h>

namespace Cluiche { namespace AppFlow {

// Owns a DebugLayerManager. Calls Draw() each frame so registered IVisualDebugger
// instances render into FrameData. Lives on MainPU (single-PU stages) or SimPU.
// Pass &GetLayerManager() to VisualDebuggerConsoleModule in the stage setup.
class VisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit VisualDebuggerModule(const Dia::Core::StringCRC& instanceId);

    Dia::Debug::DebugLayerManager& GetLayerManager() { return mLayerManager; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::Debug::DebugLayerManager mLayerManager;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
