#include "DiaUIUltralight/UIHandler.h"

#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace UI
	{
		namespace Ultralight
		{
			UIHandler::UIHandler()
				: mUISystem(nullptr)
			{}

			void UIHandler::SetUISystem(IUISystem* uiSystem)
			{
				mUISystem = uiSystem;
			}

			void UIHandler::Load(const Dia::Core::StringCRC& assetId,
			                     const Dia::Core::Containers::String512& /*resolvedPath*/,
			                     Dia::AssetRuntime::IAssetLoadCallback* callback)
			{
				if (!mUISystem)
				{
					Dia::Core::Log::OutputVaradicLine("[ERROR][UIHandler] No UI system set for asset '%u'", assetId.Value());
					callback->OnLoadFailed(assetId, "no UI system");
					return;
				}

				callback->OnLoadComplete(assetId);
			}

			void UIHandler::Unload(const Dia::Core::StringCRC& /*assetId*/)
			{
			}
		}
	}
}
