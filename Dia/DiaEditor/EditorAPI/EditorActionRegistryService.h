#pragma once

#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		// Thin service wrapper so EditorActionRegistry can be located via PluginServiceLocator.
		// Registered by EditorActionModule at DoStart; plugins access via GetServices().
		class EditorActionRegistryService
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;  // StringCRC("EditorActionRegistryService")

			explicit EditorActionRegistryService(EditorActionRegistry* registry)
				: mRegistry(registry)
			{}

			EditorActionRegistry* GetRegistry() const { return mRegistry; }

		private:
			EditorActionRegistry* mRegistry = nullptr;
		};

	} // namespace Editor
} // namespace Dia
