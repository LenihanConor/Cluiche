////////////////////////////////////////////////////////////////////////////////
// Filename: SchemaControl.h
// Description: Builds panel control JSON that carries an optional "schemaKey"
//              (full DiaValueSchema key) and "packId". Created by the domain/host
//              and passed to the code that builds GetJSONState (no global state).
//              With a resolver, each distinct key is checked once; an unresolved
//              key logs ONE Warning (channel "VSC") and bumps the metric
//              "valueschema.panel_unresolved_keys". Without a resolver the key is
//              still emitted and no check happens.
// Spec: DiaValueSchema Feature 3 (tooltips), Task 4.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace VisualDebugger
    {
        class ISchemaKeyResolver;

        class SchemaControl
        {
        public:
            // Distinct keys remembered for once-per-key checking. Keys beyond the cap are
            // not checked or logged again; they are counted in OverflowCount().
            static const unsigned int kMaxSeenKeys = 128;

            explicit SchemaControl(const ISchemaKeyResolver* resolver = nullptr);

            void SetResolver(const ISchemaKeyResolver* resolver);
            bool HasResolver() const { return mResolver != nullptr; }

            // Appends { name, type, value, schemaKey[, packId] } to the controls array.
            // type defaults to "slider"; packId is emitted only when non-null and non-empty.
            void Add(Json::Value& controls, const char* name, Dia::Core::StringCRC schemaKey,
                     float value, const char* packId = nullptr, const char* type = "slider");

            // Sets state["schemaVersion"] when a resolver is present; otherwise leaves state untouched.
            void AddSchemaVersion(Json::Value& state) const;

            unsigned int UnresolvedCount() const { return mUnresolvedCount; }
            unsigned int OverflowCount() const   { return mOverflowCount; }

        private:
            void CheckKey(Dia::Core::StringCRC schemaKey);

            const ISchemaKeyResolver*   mResolver;
            unsigned int                mSeenKeys[kMaxSeenKeys];
            unsigned int                mSeenCount;
            unsigned int                mUnresolvedCount;
            unsigned int                mOverflowCount;
        };
    }
}

#endif // DIA_DEBUG
