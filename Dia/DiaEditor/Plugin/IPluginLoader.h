#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaEditor/Project/GameSwitchGate.h>

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

			// Game switch support. Safe defaults so fakes and mocks need not implement them.
			virtual unsigned int CollectDirtyGameScoped(SwitchDirtyEntry* out, unsigned int max) { (void)out; (void)max; return 0; }
			virtual bool SaveLoadedPlugin(const Dia::Core::StringCRC& typeId) { (void)typeId; return true; }
		};
	}
}
