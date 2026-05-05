#pragma once

namespace Dia
{
    namespace AssetRuntime
    {
        class AssetRuntime;

        // Registers all asset_runtime.* DiaAPI commands.
        // Call once after initialising Dia::API and loading the manifest.
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime);
    }
}
