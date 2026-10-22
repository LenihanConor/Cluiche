#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/SchemaControl.h>
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
    namespace VisualDebugger
    {
        SchemaControl::SchemaControl(const ISchemaKeyResolver* resolver)
            : mResolver(resolver), mSeenCount(0), mUnresolvedCount(0), mOverflowCount(0)
        {
        }

        void SchemaControl::SetResolver(const ISchemaKeyResolver* resolver)
        {
            mResolver = resolver;
        }

        void SchemaControl::Add(Json::Value& controls, const char* name, Dia::Core::StringCRC schemaKey,
                                float value, const char* packId, const char* type)
        {
            Json::Value control(Json::objectValue);
            control["name"]      = name;
            control["type"]      = type;
            control["value"]     = value;
            control["schemaKey"] = schemaKey.AsChar();
            if (packId != nullptr && packId[0] != '\0')
            {
                control["packId"] = packId;
            }
            controls.append(control);

            if (mResolver != nullptr)
            {
                CheckKey(schemaKey);
            }
        }

        void SchemaControl::AddSchemaVersion(Json::Value& state) const
        {
            if (mResolver != nullptr)
            {
                state["schemaVersion"] = mResolver->Version();
            }
        }

        void SchemaControl::CheckKey(Dia::Core::StringCRC schemaKey)
        {
            const unsigned int id = schemaKey.Value();
            for (unsigned int i = 0; i < mSeenCount; ++i)
            {
                if (mSeenKeys[i] == id)
                {
                    return;
                }
            }
            if (mSeenCount >= kMaxSeenKeys)
            {
                ++mOverflowCount;
                return;
            }
            mSeenKeys[mSeenCount++] = id;

            if (!mResolver->Resolves(schemaKey))
            {
                ++mUnresolvedCount;
                static Dia::Observation::Metric::Counter* sUnresolved =
                    Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(
                        Dia::Core::StringCRC("valueschema.panel_unresolved_keys"));
                sUnresolved->Inc();
                DIA_LOG_WARNING("VSC", "Panel control schemaKey '%s' does not resolve in the value schema", schemaKey.AsChar());
            }
        }
    }
}

#endif // DIA_DEBUG
