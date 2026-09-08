#pragma once

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia
{
    namespace TriggerScript
    {
        enum class TriggerType
        {
            kSpatial,
            kTemporal,
            kState,
            kCount
        };

        struct SpatialParams
        {
            Dia::Geometry2D::AARect      region;
            Dia::Core::StringCRC         entityTag;  // empty = any entity
        };

        struct TemporalParams
        {
            float intervalSeconds = 0.0f;
        };

        struct StateParams
        {
            Dia::Condition::ConditionExpr condition;

            StateParams() = default;
            ~StateParams() = default;

            StateParams(StateParams&&) = default;
            StateParams& operator=(StateParams&&) = default;

            StateParams(const StateParams&) = delete;
            StateParams& operator=(const StateParams&) = delete;
        };

        struct CountParams
        {
            Dia::Core::StringCRC entityTag;
            int                  threshold = 0;
        };

        struct ActionDef
        {
            Dia::Core::StringCRC actionType;
            Json::Value          params;
        };

        struct TriggerDef
        {
            Dia::Core::StringCRC id;
            TriggerType          type          = TriggerType::kState;
            bool                 oneShot       = true;
            float                checkIntervalMs = 0.0f;

            SpatialParams  spatial;
            TemporalParams temporal;
            StateParams    state;
            CountParams    count;

            Dia::Core::Containers::DynamicArrayC<ActionDef, 8> actions;

            TriggerDef() = default;
            ~TriggerDef() = default;

            TriggerDef(TriggerDef&&) = default;
            TriggerDef& operator=(TriggerDef&&) = default;

            TriggerDef(const TriggerDef&) = delete;
            TriggerDef& operator=(const TriggerDef&) = delete;
        };

    } // namespace TriggerScript
} // namespace Dia
