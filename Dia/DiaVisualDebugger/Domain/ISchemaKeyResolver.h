////////////////////////////////////////////////////////////////////////////////
// Filename: ISchemaKeyResolver.h
// Description: Tiny seam so SchemaControl can check schema keys without
//              DiaVisualDebugger depending on DiaValueSchema. The host wraps its
//              ValueSchemaService (read from the "ValueSchemaService" ServiceStream)
//              in an adapter: Resolves -> service.Find(key) != nullptr,
//              Version -> service.Version().
// Spec: DiaValueSchema Feature 3 (tooltips), Task 4.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace VisualDebugger
    {
        class ISchemaKeyResolver
        {
        public:
            virtual ~ISchemaKeyResolver() {}
            virtual bool         Resolves(Dia::Core::StringCRC fullKey) const = 0;
            virtual unsigned int Version() const = 0;
        };
    }
}

#endif // DIA_DEBUG
