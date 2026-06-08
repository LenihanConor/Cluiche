////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistry3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaCamera3D/Camera3D.h"
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		/// \brief Named registry for 3D cameras with composable behaviours.
		///
		/// Owns cameras and their attached behaviours (deletes on Unregister).
		/// Behaviours tick in attachment order each frame via UpdateAll(dt).
		///
		/// Exactly one camera must be active before calling GetActive().
		////////////////////////////////////////////////////////////
		class CameraRegistry3D
		{
		public:
			static constexpr unsigned int kMaxCameras    = 8;
			static constexpr unsigned int kMaxBehaviours = 8;

			CameraRegistry3D();
			~CameraRegistry3D();

			// Non-copyable (owns behaviour pointers)
			CameraRegistry3D(const CameraRegistry3D&) = delete;
			CameraRegistry3D& operator=(const CameraRegistry3D&) = delete;

			// Registration
			bool Register  (Dia::Core::StringCRC id, const Camera3D& camera);
			void Unregister(Dia::Core::StringCRC id);
			bool Has       (Dia::Core::StringCRC id) const;

			// Behaviour management (registry takes ownership, deletes on Unregister)
			bool AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour3D* behaviour);
			void DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId);

			// Active camera
			void                  SetActive   (Dia::Core::StringCRC id);
			const Camera3D&       GetActive   () const;
			Camera3D&             GetActive   ();
			Dia::Core::StringCRC  GetActiveId () const;

			// Direct access
			const Camera3D& Get(Dia::Core::StringCRC id) const;
			Camera3D&       Get(Dia::Core::StringCRC id);

			// Tick all cameras' behaviours in attachment order
			void UpdateAll(float dt);

			unsigned int GetCount() const { return mCount; }

		private:
			int FindIndex(Dia::Core::StringCRC id) const;
			void DeleteBehaviours(unsigned int slotIndex);

			struct CameraSlot
			{
				Dia::Core::StringCRC  id;
				Camera3D              camera;
				ICameraBehaviour3D*   behaviours[kMaxBehaviours];
				unsigned int          behaviourCount = 0;
			};

			CameraSlot   mSlots[kMaxCameras];
			unsigned int mCount       = 0;
			int          mActiveIndex = -1;
		};

	} // namespace Camera3D
} // namespace Dia
