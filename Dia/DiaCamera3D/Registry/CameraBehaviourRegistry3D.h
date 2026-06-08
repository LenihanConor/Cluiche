////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBehaviourRegistry3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		/// \brief Self-registering factory for 3D camera behaviours.
		///
		/// Each behaviour registers its factory via a static initializer in its .cpp.
		/// The loader calls Create(typeId, config) without mentioning specific types.
		////////////////////////////////////////////////////////////
		class CameraBehaviourRegistry3D
		{
		public:
			using FactoryFn = ICameraBehaviour3D* (*)(const void* config);

			static constexpr unsigned int kMaxBehaviourTypes = 16;

			static CameraBehaviourRegistry3D& Get();

			void                 Register    (Dia::Core::StringCRC typeId, FactoryFn factory);

			/// Create a behaviour by type ID. Returns nullptr if unknown.
			ICameraBehaviour3D*  Create      (Dia::Core::StringCRC typeId, const void* config = nullptr) const;

			bool                 IsRegistered(Dia::Core::StringCRC typeId) const;

		private:
			CameraBehaviourRegistry3D() = default;

			struct Entry
			{
				Dia::Core::StringCRC typeId;
				FactoryFn            factory = nullptr;
			};

			Entry        mEntries[kMaxBehaviourTypes];
			unsigned int mCount = 0;
		};

	} // namespace Camera3D
} // namespace Dia
