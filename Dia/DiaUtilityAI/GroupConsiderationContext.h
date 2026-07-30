#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace UtilityAI
    {
        // Tracks active action counts across a group (e.g. a squad).
        // Game code owns and drives Increment/Decrement; UtilitySet reads counts
        // during eligibility checks. NOT a singleton -- caller creates and owns.
        class GroupConsiderationContext
        {
        public:
            GroupConsiderationContext();
            ~GroupConsiderationContext();

            void Increment(Dia::Core::StringCRC actionId);
            void Decrement(Dia::Core::StringCRC actionId);
            int  GetCount(Dia::Core::StringCRC actionId) const;
            void Reset();

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace UtilityAI
} // namespace Dia
