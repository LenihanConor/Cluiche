#include "DiaAnimation2D/AnimClipBusAdapter.h"
#include "DiaAnimation2D/AnimClip.h"
#include "DiaAnimation2D/Messages/animation2d_messages.h"
#include <DiaMessageBus/Bus.h>
#include <DiaEntity/EntityAddress.h>

namespace Dia { namespace Animation2D {

    AnimClipBusAdapter::AnimClipBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner)
        : mBus(bus)
        , mOwner(owner)
    {
        // Registers both message types + producer metadata. RegisterType is a
        // no-op (returns false, no assert) if already registered, so this is
        // safe even if something else already registered these types.
        Messages::RegisterMessages(mBus, Messages::Handlers{});
    }

    void AnimClipBusAdapter::OnClipFinished(const AnimClip& clip)
    {
        Messages::ClipFinishedEvent e;
        e.clipId = clip.GetId();

        if (mOwner.IsValid())
        {
            mBus.Post<Messages::ClipFinishedEvent>(Dia::Entity::MakeEntityAddress(mOwner), e);
        }
        else
        {
            mBus.Broadcast(e);
        }
    }

    void AnimClipBusAdapter::OnClipLooped(const AnimClip& clip)
    {
        Messages::ClipLoopedEvent e;
        e.clipId = clip.GetId();

        if (mOwner.IsValid())
        {
            mBus.Post<Messages::ClipLoopedEvent>(Dia::Entity::MakeEntityAddress(mOwner), e);
        }
        else
        {
            mBus.Broadcast(e);
        }
    }

}} // namespace Dia::Animation2D
