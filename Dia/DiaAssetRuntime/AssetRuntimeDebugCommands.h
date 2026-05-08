#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
    namespace AssetRuntime
    {
        class AssetRuntime;

        class ITransitionNotifier
        {
        public:
            virtual ~ITransitionNotifier() = default;
            virtual void Notify(const Dia::Core::StringCRC& topic, const char* jsonPayload) = 0;
        };

        // Registers all asset_runtime.* DiaAPI commands.
        // Call once after initialising Dia::API and loading the manifest.
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime, ITransitionNotifier* notifier = nullptr);
    }
}
