////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBehaviourRegistry.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

namespace Dia { namespace Camera2D { class ICameraBehaviour; } }

namespace Dia
{
	namespace Camera2D
	{
		struct ReflectedConfig; // forward — defined by DiaReflect; used by factory

		using BehaviourFactory = ICameraBehaviour* (*)(const void* config);

		////////////////////////////////////////////////////////////
		/// \brief Self-registering factory for camera behaviours.
		///
		/// Each behaviour registers its factory via a static initializer in its .cpp.
		/// The loader calls Create(typeId, config) without mentioning specific types.
		////////////////////////////////////////////////////////////
		class CameraBehaviourRegistry
		{
		public:
			static CameraBehaviourRegistry& Get();

			void Register(Dia::Core::StringCRC typeId, BehaviourFactory factory);

			/// Create a behaviour by type ID. Returns nullptr if unknown.
			ICameraBehaviour* Create(Dia::Core::StringCRC typeId, const void* config = nullptr) const;

			bool IsRegistered(Dia::Core::StringCRC typeId) const;

		private:
			CameraBehaviourRegistry() = default;

			static const unsigned int kMaxBehaviourTypes = 16;
			Dia::Core::StringCRC mTypeIds[kMaxBehaviourTypes];
			BehaviourFactory     mFactories[kMaxBehaviourTypes];
			unsigned int         mCount = 0;
		};

	} // namespace Camera2D
} // namespace Dia
