#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Objective
    {
        struct RewardEntry
        {
            Dia::Core::StringCRC type;
            float                amount;
        };

        using RewardPayload = Dia::Core::Containers::DynamicArrayC<RewardEntry, 8>;

        class IObjectiveObserver
        {
        public:
            virtual ~IObjectiveObserver() = default;

            virtual void OnObjectiveActivated(Dia::Core::StringCRC objectiveId) = 0;
            virtual void OnObjectiveCompleted(Dia::Core::StringCRC objectiveId,
                                              const RewardPayload& reward) = 0;
            virtual void OnObjectiveFailed   (Dia::Core::StringCRC objectiveId) = 0;
        };

    } // namespace Objective
} // namespace Dia
