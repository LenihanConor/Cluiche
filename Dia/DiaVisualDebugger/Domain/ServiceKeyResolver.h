////////////////////////////////////////////////////////////////////////////////
// Filename: ServiceKeyResolver.h
// Description: Header-only ISchemaKeyResolver adapter over any "service" type that
//              has `Find(StringCRC) const` (non-null = key exists) and
//              `unsigned Version() const`. The host instantiates it with
//              Dia::ValueSchema::ValueSchemaService, so DiaVisualDebugger needs no
//              DiaValueSchema dependency.
//
//              The service pointer is not owned and may be re-pointed (it changes on
//              every module start/restart). With no service the adapter reports
//              IsAvailable() == false; SchemaControl then behaves exactly as if it
//              had no resolver (no key checks, no schemaVersion). A host can pass
//              either nullptr or this adapter; prefer the adapter so availability
//              is picked up when the service appears later.
//
//              Threading: SetService is called by the owner of the service stream
//              (Main PU in the panel host); readers on another PU must tolerate the
//              pointer changing between frames.
// Spec: DiaValueSchema Feature 3 (tooltips), Task 4b.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>

namespace Dia
{
    namespace VisualDebugger
    {
        template <class Service>
        class ServiceKeyResolver : public ISchemaKeyResolver
        {
        public:
            explicit ServiceKeyResolver(const Service* service = nullptr) : mService(service) {}

            void           SetService(const Service* service) { mService = service; }
            const Service* GetService() const                 { return mService; }

            bool IsAvailable() const override { return mService != nullptr; }

            bool Resolves(Dia::Core::StringCRC fullKey) const override
            {
                return mService != nullptr && mService->Find(fullKey) != nullptr;
            }

            unsigned int Version() const override
            {
                return mService != nullptr ? mService->Version() : 0u;
            }

        private:
            const Service* mService;
        };
    }
}

#endif // DIA_DEBUG
