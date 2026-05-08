#pragma once

#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace UI
	{
		class IUISystem;

		namespace Ultralight
		{
			class UIHandler : public Dia::AssetRuntime::IAssetTypeHandler
			{
			public:
				UIHandler();

				void SetUISystem(IUISystem* uiSystem);

				virtual void Load(const Dia::Core::StringCRC& assetId,
				                  const Dia::Core::Containers::String512& resolvedPath,
				                  Dia::AssetRuntime::IAssetLoadCallback* callback) override;

				virtual void Unload(const Dia::Core::StringCRC& assetId) override;

			private:
				IUISystem* mUISystem;
			};
		}
	}
}
