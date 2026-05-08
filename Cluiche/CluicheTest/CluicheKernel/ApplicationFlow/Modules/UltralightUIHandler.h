#pragma once

#include <DiaAssetRuntime/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>
#include <DiaLogger/DiaLog.h>

namespace Dia { namespace UI { class IUISystem; } }

namespace Cluiche
{
	namespace Main
	{
		class UltralightUIHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			UltralightUIHandler() : mUISystem(nullptr) {}

			void SetUISystem(Dia::UI::IUISystem* uiSystem) { mUISystem = uiSystem; }

			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
			{
				if (!mUISystem)
				{
					DIA_LOG_ERROR("UltralightUIHandler", "No UI system set for asset '%s'", assetId.AsChar());
					callback->OnLoadFailed(assetId, "no UI system");
					return;
				}

				DIA_LOG_DEBUG("UltralightUIHandler", "Loading UI asset '%s' from '%s'", assetId.AsChar(), resolvedPath.AsCStr());
				callback->OnLoadComplete(assetId);
			}

			virtual void Unload(const Dia::Core::StringCRC& assetId) override
			{
				DIA_LOG_DEBUG("UltralightUIHandler", "Unloading UI asset '%s'", assetId.AsChar());
			}

		private:
			Dia::UI::IUISystem* mUISystem;
		};
	}
}
