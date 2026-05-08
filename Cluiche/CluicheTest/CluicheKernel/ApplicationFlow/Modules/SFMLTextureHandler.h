#pragma once

#include <DiaAssetRuntime/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>
#include <DiaLogger/DiaLog.h>

namespace Dia { namespace SFML { class RenderWindow; } }

namespace Cluiche
{
	namespace Main
	{
		class SFMLTextureHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			SFMLTextureHandler() : mRenderWindow(nullptr) {}

			void SetRenderWindow(Dia::SFML::RenderWindow* renderWindow) { mRenderWindow = renderWindow; }

			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override
			{
				if (!mRenderWindow)
				{
					DIA_LOG_ERROR("SFMLTextureHandler", "No render window set for asset '%s'", assetId.AsChar());
					callback->OnLoadFailed(assetId, "no render window");
					return;
				}

				DIA_LOG_DEBUG("SFMLTextureHandler", "Loading texture '%s' from '%s'", assetId.AsChar(), resolvedPath.AsCStr());
				callback->OnLoadComplete(assetId);
			}

			virtual void Unload(const Dia::Core::StringCRC& assetId) override
			{
				DIA_LOG_DEBUG("SFMLTextureHandler", "Unloading texture '%s'", assetId.AsChar());
			}

		private:
			Dia::SFML::RenderWindow* mRenderWindow;
		};
	}
}
