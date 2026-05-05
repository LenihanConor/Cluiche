#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			struct SharedPluginState
			{
				Dia::Core::StringCRC mSelectedAssetId;
				bool mConnected;

				SharedPluginState()
					: mSelectedAssetId()
					, mConnected(false)
				{}
			};
		}
	}
}
