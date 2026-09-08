#include "DiaBlackboard/GlobalBlackboard.h"

namespace Dia { namespace Blackboard {

    GlobalBlackboard::GlobalBlackboard()
    {
    }

    Blackboard& GlobalBlackboard::GetBoard()
    {
        return mBoard;
    }

    const Blackboard& GlobalBlackboard::GetBoard() const
    {
        return mBoard;
    }

}}
