#include "DiaInput/InputBusAdapter.h"

#include <DiaMessageBus/Bus.h>

namespace Dia::Input {

InputBusAdapter::InputBusAdapter(InputSourceManager& inputSourceManager, Dia::MessageBus::Bus& bus)
    : mInputSourceManager(inputSourceManager)
{
    // RegisterMessages is a no-op (returns false, no assert) if these types
    // are already registered, so this is safe even if something else
    // already registered them.
    Messages::RegisterMessages(bus, Messages::Handlers{});
}

void InputBusAdapter::Flush(Dia::MessageBus::Bus& bus)
{
    mScratch.RemoveAll(); // Update() appends — not cleared by the callee.
    mInputSourceManager.Update(mScratch);

    for (unsigned int i = 0; i < mScratch.Size(); ++i)
    {
        const Event& ev = mScratch[i];

        if (ev.type == Event::EType::kKeyPressed)
        {
            Messages::KeyDownEvent msg;
            msg.code    = ev.key.code;
            msg.alt     = ev.key.alt;
            msg.control = ev.key.control;
            msg.shift   = ev.key.shift;
            msg.system  = ev.key.system;
            bus.Broadcast(msg);
        }
        else if (ev.type == Event::EType::kKeyReleased)
        {
            Messages::KeyUpEvent msg;
            msg.code    = ev.key.code;
            msg.alt     = ev.key.alt;
            msg.control = ev.key.control;
            msg.shift   = ev.key.shift;
            msg.system  = ev.key.system;
            bus.Broadcast(msg);
        }
        else if (ev.type == Event::EType::kMouseButtonPressed)
        {
            Messages::MouseButtonEvent msg;
            msg.button  = ev.mouseButton.button;
            msg.x       = ev.mouseButton.x;
            msg.y       = ev.mouseButton.y;
            msg.pressed = true;
            bus.Broadcast(msg);
        }
        else if (ev.type == Event::EType::kMouseButtonReleased)
        {
            Messages::MouseButtonEvent msg;
            msg.button  = ev.mouseButton.button;
            msg.x       = ev.mouseButton.x;
            msg.y       = ev.mouseButton.y;
            msg.pressed = false;
            bus.Broadcast(msg);
        }
        else if (ev.type == Event::EType::kMouseMoved)
        {
            Messages::MouseMovedEvent msg;
            msg.x = ev.mouseMove.x;
            msg.y = ev.mouseMove.y;
            bus.Broadcast(msg);
        }
        // Window/joystick/gamepad events are not yet modeled on the bus.
    }
}

} // namespace Dia::Input
