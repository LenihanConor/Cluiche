#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/DebugUIModule.h"

namespace CluicheTest {

class TestStageHUDModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit TestStageHUDModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RenderBottomBar();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
};

} // namespace CluicheTest
