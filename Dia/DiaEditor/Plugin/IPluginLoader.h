#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		class IPluginLoader
		{
		public:
			virtual ~IPluginLoader() = default;
			virtual void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId) = 0;
			virtual bool UnloadPlugin(const Dia::Core::StringCRC& typeId) = 0;
			virtual bool IsPluginTypeLoaded(const Dia::Core::StringCRC& typeId) const = 0;
			virtual bool IsPluginPinned(const Dia::Core::StringCRC& typeId) const = 0;
		};
	}
}
