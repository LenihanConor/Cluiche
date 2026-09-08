#pragma once
#include "Modules/MainStateProducerModule.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>

namespace CluicheTest {

class TestMainStateProducerModule : public Cluiche::AppFlow::MainStateProducerModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Populates MainToRender with test stage HUD data";
    explicit TestMainStateProducerModule(const Dia::Core::StringCRC& instanceId);

protected:
    void DoPopulateFrame(Cluiche::AppFlow::MainToRenderFrame& frame) override;

};

} // namespace CluicheTest
