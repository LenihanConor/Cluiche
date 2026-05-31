////////////////////////////////////////////////////////////////////////////////
// Filename: PickEvent.h
// Description: Message type sent through DiaMailbox when a pick query fires.
//              PickingModule sends one PickEvent per trigger per frame.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaPicking/PickTrigger.h>
#include <DiaPicking/PickResult.h>

namespace Dia::Picking {

template<typename THit, unsigned int MaxHits = 16>
struct PickEvent
{
    PickTrigger              trigger;
    PickResult<THit, MaxHits> hits;
};

} // namespace Dia::Picking
