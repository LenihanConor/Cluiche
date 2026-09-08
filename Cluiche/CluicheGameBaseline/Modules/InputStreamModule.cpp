#include "Modules/InputStreamModule.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/Event.h>

#include <cstring>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC InputStreamModule::kTypeId("InputStreamModule");

InputStreamModule::InputStreamModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
    memset(mCurrentMouse,  0, sizeof(mCurrentMouse));
    memset(mPreviousMouse, 0, sizeof(mPreviousMouse));
}

Dia::ApplicationFlow::StartResult InputStreamModule::DoStart()
{
    DIA_LOG_INFO("Application", "InputStreamModule::DoStart entry");
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
    memset(mCurrentMouse,  0, sizeof(mCurrentMouse));
    memset(mPreviousMouse, 0, sizeof(mPreviousMouse));
    DIA_LOG_INFO("Application", "InputStreamModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void InputStreamModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    memcpy(mPreviousKeys,  mCurrentKeys,  sizeof(mCurrentKeys));
    memcpy(mPreviousMouse, mCurrentMouse, sizeof(mCurrentMouse));

    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<MainToSimEvent>, 64> events;
    mInput.Consume(events);

    for (unsigned int i = 0; i < static_cast<unsigned int>(events.Size()); ++i)
    {
        const MainToSimEvent& envelope = events[i].payload;
        if (envelope.kind != MainToSimEvent::Kind::kInput)
            continue;

        const InputEvent& evt = envelope.input;
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
        else if (evt.type == InputEvent::EType::kMouseButtonPressed)
        {
            int btn = evt.mouseButton.button;
            if (btn >= 0 && static_cast<unsigned int>(btn) < kMaxMouseButtons)
                mCurrentMouse[btn] = true;
        }
        else if (evt.type == InputEvent::EType::kMouseButtonReleased)
        {
            int btn = evt.mouseButton.button;
            if (btn >= 0 && static_cast<unsigned int>(btn) < kMaxMouseButtons)
                mCurrentMouse[btn] = false;
        }
        else if (evt.type == InputEvent::EType::kMouseMoved)
        {
            mMouseX = evt.mouseMove.x;
            mMouseY = evt.mouseMove.y;
        }
    }
}

Dia::ApplicationFlow::StopResult InputStreamModule::DoStop()
{
    DIA_LOG_INFO("Application", "InputStreamModule::DoStop entry");
    memset(mCurrentKeys,  0, sizeof(mCurrentKeys));
    memset(mPreviousKeys, 0, sizeof(mPreviousKeys));
    memset(mCurrentMouse,  0, sizeof(mCurrentMouse));
    memset(mPreviousMouse, 0, sizeof(mPreviousMouse));
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

bool InputStreamModule::IsMouseButtonDown(int button) const
{
    if (button < 0 || static_cast<unsigned int>(button) >= kMaxMouseButtons) return false;
    return mCurrentMouse[button];
}

bool InputStreamModule::WasMouseButtonPressed(int button) const
{
    if (button < 0 || static_cast<unsigned int>(button) >= kMaxMouseButtons) return false;
    return !mPreviousMouse[button] && mCurrentMouse[button];
}

bool InputStreamModule::WasMouseButtonReleased(int button) const
{
    if (button < 0 || static_cast<unsigned int>(button) >= kMaxMouseButtons) return false;
    return mPreviousMouse[button] && !mCurrentMouse[button];
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using InputStreamModule_ = Cluiche::AppFlow::InputStreamModule; }
DIA_MODULE(InputStreamModule_);
DIA_DESCRIBE(InputStreamModule_::kTypeId, "Polls raw input devices and publishes input state to consumers via FrameStream.");
