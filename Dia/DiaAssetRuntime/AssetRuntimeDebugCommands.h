#pragma once

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>
#include <functional>

namespace Dia
{
    namespace AssetRuntime
    {
        class AssetRuntime;

        using TransitionNotifyCallback = std::function<void(const Dia::Core::StringCRC& topic, const Json::Value& payload)>;

        // Registers all asset_runtime.* DiaAPI commands.
        // Call once after initialising Dia::API and loading the manifest.
        void RegisterAssetRuntimeCommands(AssetRuntime& runtime, TransitionNotifyCallback notifyCallback = nullptr);
    }
}
