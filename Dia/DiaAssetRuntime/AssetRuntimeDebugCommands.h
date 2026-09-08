#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace DebugServer { class QueryRegistry; }

    namespace AssetRuntime
    {
        class AssetRuntime;

        class ITransitionNotifier
        {
        public:
            virtual ~ITransitionNotifier() = default;
            virtual void Notify(const Dia::Core::StringCRC& topic, const char* jsonPayload) = 0;
        };

        // Registers all asset_runtime.* DiaAPI commands (CLI usage).
        // Call once after initialising Dia::API and loading the manifest.
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime, ITransitionNotifier* notifier = nullptr);

        // Registers structured query handlers for remote editor polling.
        void RegisterAssetRuntimeQueryHandlers(Dia::DebugServer::QueryRegistry& registry, const AssetRuntime& runtime);
    }
}
