#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/EventStreamReader.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaInput/EKey.h>
#include "Types/InputEvent.h"
#include "Modules/TimeServerModule.h"

namespace Cluiche { namespace AppFlow {

class InputStreamModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit InputStreamModule(const Dia::Core::StringCRC& instanceId);

    bool IsKeyDown(Dia::Input::EKey key)      const;
    bool WasKeyPressed(Dia::Input::EKey key)  const;
    bool WasKeyReleased(Dia::Input::EKey key) const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()                              override;
    void                              DoUpdate(float dt)                     override;
    Dia::ApplicationFlow::StopResult  DoStop()                               override;
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    static constexpr unsigned int kMaxKeys = 256;

    Dia::ApplicationFlow::EventStreamReader<InputEvent> mInput{this, "InputToSim"};
    Dia::ApplicationFlow::ModuleRef<TimeServerModule>   mTimeServer{this};

    bool mCurrentKeys[kMaxKeys];
    bool mPreviousKeys[kMaxKeys];
};

} } // namespace Cluiche::AppFlow
