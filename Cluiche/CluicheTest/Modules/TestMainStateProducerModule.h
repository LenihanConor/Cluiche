#pragma once
#include "Modules/MainStateProducerModule.h"
#include <DiaCore/CRC/StringCRC.h>

namespace CluicheTest {

class TestMainStateProducerModule : public Cluiche::AppFlow::MainStateProducerModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit TestMainStateProducerModule(const Dia::Core::StringCRC& instanceId);

protected:
    void DoPopulateFrame(Cluiche::AppFlow::MainToRenderFrame& frame) override;

private:
    Dia::Core::StringCRC mHUDWarnedStage;
};

} // namespace CluicheTest
