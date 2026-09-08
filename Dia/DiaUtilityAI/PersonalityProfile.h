#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace UtilityAI {

    struct ActionBias {
        Dia::Core::StringCRC actionId;
        float scoreMultiplier = 1.0f;
    };

    // Immutable after LoadFromJson. Shared across entities with the same archetype.
    class PersonalityProfile {
    public:
        PersonalityProfile();

        static PersonalityProfile LoadFromJson(const Json::Value& root);

        Dia::Core::StringCRC GetName() const;
        // Returns 1.0f if action has no bias entry (FD-002).
        float GetScoreMultiplier(Dia::Core::StringCRC actionId) const;
        // Default: 1 if absent from JSON (FD-004).
        int   GetEvalPeriodTicks() const;
        bool  IsValid() const;

    private:
        Dia::Core::StringCRC mName;
        Dia::Core::Containers::DynamicArrayC<ActionBias, 16> mBiases;
        int  mEvalPeriodTicks;
        bool mValid;
    };

}} // namespace Dia::UtilityAI
