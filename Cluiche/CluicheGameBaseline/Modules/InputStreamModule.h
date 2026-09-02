#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaInput/EKey.h>
#include "Types/MainToSimEvent.h"

namespace Cluiche { namespace AppFlow {

class InputStreamModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Reads input events from MainToSim stream";
    explicit InputStreamModule(const Dia::Core::StringCRC& instanceId);

    bool IsKeyDown(Dia::Input::EKey key)      const;
    bool WasKeyPressed(Dia::Input::EKey key)  const;
    bool WasKeyReleased(Dia::Input::EKey key) const;

    bool IsMouseButtonDown(int button)      const;
    bool WasMouseButtonPressed(int button)  const;
    bool WasMouseButtonReleased(int button) const;

    int GetMouseX() const { return mMouseX; }
    int GetMouseY() const { return mMouseY; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()                              override;
    void                              DoUpdate(float dt)                     override;
    Dia::ApplicationFlow::StopResult  DoStop()                               override;
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    static constexpr unsigned int kMaxKeys         = 256;
    static constexpr unsigned int kMaxMouseButtons = 8;

    Dia::ApplicationFlow::EventStreamReader<MainToSimEvent> mInput{this, "MainToSim"};

    bool mCurrentKeys[kMaxKeys];
    bool mPreviousKeys[kMaxKeys];
    bool mCurrentMouse[kMaxMouseButtons];
    bool mPreviousMouse[kMaxMouseButtons];
    int  mMouseX = 0;
    int  mMouseY = 0;
};

} } // namespace Cluiche::AppFlow
