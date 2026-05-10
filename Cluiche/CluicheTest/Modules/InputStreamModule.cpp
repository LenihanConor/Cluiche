#include "Modules/InputStreamModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaApplicationFlow/Application.h>

#include <cstring>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC InputStreamModule::kTypeId("InputStreamModule");

InputStreamModule::InputStreamModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
}

Dia::ApplicationFlow::StartResult InputStreamModule::DoStart()
{
    DIA_LOG_INFO("Application", "InputStreamModule::DoStart entry");
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
    DIA_LOG_INFO("Application", "InputStreamModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void InputStreamModule::DoUpdate(float /*dt*/)
{
    memcpy(mPreviousKeys, mCurrentKeys, sizeof(mCurrentKeys));

    Dia::Core::Containers::DynamicArrayC<InputEvent, 64> events;
    mInput.Consume(events);

    for (unsigned int i = 0; i < static_cast<unsigned int>(events.Size()); ++i)
    {
        const InputEvent& evt = events[i];
        if (evt.type == InputEvent::EType::kKeyPressed)
        {
            unsigned int idx = static_cast<unsigned int>(evt.key.code);
            if (idx < kMaxKeys)
                mCurrentKeys[idx] = true;
        }
        else if (evt.type == InputEvent::EType::kKeyReleased)
        {
            unsigned int idx = static_cast<unsigned int>(evt.key.code);
            if (idx < kMaxKeys)
                mCurrentKeys[idx] = false;
        }
    }
}

Dia::ApplicationFlow::StopResult InputStreamModule::DoStop()
{
    DIA_LOG_INFO("Application", "InputStreamModule::DoStop entry");
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
    return Dia::ApplicationFlow::StopResult::kDone;
}

void InputStreamModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mInput.Connect(app);
}

bool InputStreamModule::IsKeyDown(Dia::Input::EKey key) const
{
    unsigned int idx = static_cast<unsigned int>(static_cast<int>(key));
    if (idx >= kMaxKeys) return false;
    return mCurrentKeys[idx];
}

bool InputStreamModule::WasKeyPressed(Dia::Input::EKey key) const
{
    unsigned int idx = static_cast<unsigned int>(static_cast<int>(key));
    if (idx >= kMaxKeys) return false;
    return !mPreviousKeys[idx] && mCurrentKeys[idx];
}

bool InputStreamModule::WasKeyReleased(Dia::Input::EKey key) const
{
    unsigned int idx = static_cast<unsigned int>(static_cast<int>(key));
    if (idx >= kMaxKeys) return false;
    return mPreviousKeys[idx] && !mCurrentKeys[idx];
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using InputStreamModule_ = Cluiche::AppFlow::InputStreamModule; }
DIA_MODULE(InputStreamModule_);
