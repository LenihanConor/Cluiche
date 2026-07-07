#pragma once

#include "DiaCore/Architecture/Singleton/Singleton.h"
#include "DiaBlackboard/Blackboard.h"

namespace Dia { namespace Blackboard {

    class GlobalBlackboard : public Dia::Core::Singleton<GlobalBlackboard>
    {
    public:
        GlobalBlackboard();

        Blackboard& GetBoard();
        const Blackboard& GetBoard() const;

    private:
        Blackboard mBoard;
    };

}}
