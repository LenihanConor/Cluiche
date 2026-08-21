#pragma once

#include "DiaAnimation2D/IAnimClipObserver.h"
#include <DiaEntity/Entity.h>

namespace Dia { namespace MessageBus { class Bus; } }

namespace Dia { namespace Animation2D {

    // -----------------------------------------------------------------------
    // AnimClipBusAdapter
    //
    // One more IAnimClipObserver subscriber — registered on an
    // AnimClipPlayer's observer subject alongside any other direct
    // observers, it does not replace them. Forwards OnClipFinished/
    // OnClipLooped onto the shared DiaMessageBus::Bus as the codegen'd
    // Dia::Animation2D::Messages::ClipFinishedEvent/ClipLoopedEvent
    // (Messages/animation2d_messages.h), carrying AnimClip::GetId()'s
    // StringCRC — never a raw AnimClip* — since a bus-queued message can
    // outlive the frame the clip pointer was valid for.
    //
    // Entity addressing is optional and opportunistic: there is no
    // AnimationComponent2D (out of scope, see clip-completion-bus-adapter.md
    // Resolved Design Decisions) establishing a standing association between
    // an AnimClipPlayer and an entity, so the caller supplies one only if it
    // already happens to hold both. Per the resolved design decision,
    // Broadcast is the PRIMARY intended delivery mode, not a fallback taken
    // only when an owner is unavailable — "an animation finished" is a
    // legitimate thing to broadcast with no known/needed listener identity.
    // -----------------------------------------------------------------------
    class AnimClipBusAdapter : public IAnimClipObserver
    {
    public:
        // owner is optional — a default-constructed (invalid) Entity means
        // Broadcast. Registers both message types (+ producer metadata)
        // with the bus.
        explicit AnimClipBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner = Dia::Entity::Entity());

        void OnClipFinished (const AnimClip& clip) override;
        void OnClipLooped   (const AnimClip& clip) override;

    private:
        Dia::MessageBus::Bus& mBus;
        Dia::Entity::Entity   mOwner;
    };

}} // namespace Dia::Animation2D
