////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistry2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Behaviour/ICameraBehaviour.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Observation { namespace Metric { class Gauge; class Counter; } } }

namespace Dia
{
	namespace Camera2D
	{
		////////////////////////////////////////////////////////////
		/// \brief Named registry for 2D cameras with composable behaviours.
		///
		/// Owns cameras and their attached behaviours (deletes on Unregister).
		/// Behaviours tick in attachment order each frame via UpdateAll(dt).
		///
		/// Exactly one camera must be active before calling GetActive().
		////////////////////////////////////////////////////////////
		class CameraRegistry2D
		{
		public:
			static const unsigned int kMaxCameras    = 8;
			static const unsigned int kMaxBehaviours = 8;

			CameraRegistry2D();
			~CameraRegistry2D();

			// Non-copyable (owns behaviour pointers)
			CameraRegistry2D(const CameraRegistry2D&) = delete;
			CameraRegistry2D& operator=(const CameraRegistry2D&) = delete;

			// Registration
			void Register(Dia::Core::StringCRC id, Camera2D camera);
			void Unregister(Dia::Core::StringCRC id);
			bool Has(Dia::Core::StringCRC id) const;

			// Behaviour management (registry takes ownership)
			void AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour* behaviour);
			void DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId);

			// Active camera
			void          SetActive(Dia::Core::StringCRC id);
			Camera2D&     GetActive();
			const Camera2D& GetActive() const;
			Dia::Core::StringCRC GetActiveId() const;

			// Direct access
			Camera2D&     Get(Dia::Core::StringCRC id);
			const Camera2D& Get(Dia::Core::StringCRC id) const;

			// Update all cameras' behaviours in attachment order
			void UpdateAll(float dt);

			unsigned int GetCount() const { return mCount; }

		private:
			int FindIndex(Dia::Core::StringCRC id) const;
			void DeleteBehaviours(unsigned int slotIndex);

			struct CameraSlot
			{
				Dia::Core::StringCRC id;
				Camera2D             camera;
				ICameraBehaviour*    behaviours[kMaxBehaviours];
				unsigned int         behaviourCount = 0;
			};

			CameraSlot   mSlots[kMaxCameras];
			unsigned int mCount       = 0;
			int          mActiveIndex = -1;

			// Observability
			Dia::Observation::Metric::Gauge*   mMetricCameraCount    = nullptr;
			Dia::Observation::Metric::Counter* mMetricBehaviourTicks = nullptr;
		};

	} // namespace Camera2D
} // namespace Dia
