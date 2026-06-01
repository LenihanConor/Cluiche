////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistry2D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Registry/CameraRegistry2D.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
	namespace Camera2D
	{
		////////////////////////////////////////////////////////////
		CameraRegistry2D::CameraRegistry2D()
		{
			for (unsigned int i = 0; i < kMaxCameras; ++i)
			{
				mSlots[i].behaviourCount = 0;
				for (unsigned int b = 0; b < kMaxBehaviours; ++b)
					mSlots[i].behaviours[b] = nullptr;
			}

			auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
			mMetricCameraCount    = reg.RegisterGauge(Dia::Core::StringCRC("dia.camera2d.count"));
			mMetricBehaviourTicks = reg.RegisterCounter(Dia::Core::StringCRC("dia.camera2d.behaviour_ticks"));
		}

		////////////////////////////////////////////////////////////
		CameraRegistry2D::~CameraRegistry2D()
		{
			for (unsigned int i = 0; i < mCount; ++i)
				DeleteBehaviours(i);
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::Register(Dia::Core::StringCRC id, Camera2D camera)
		{
			if (Has(id))
			{
				DIA_LOG_WARNING("DiaCamera2D", "CameraRegistry2D::Register — duplicate camera id, ignoring");
				return;
			}
			DIA_ASSERT(mCount < kMaxCameras, "CameraRegistry2D::Register — capacity exceeded");

			mSlots[mCount].id             = id;
			mSlots[mCount].camera         = camera;
			mSlots[mCount].behaviourCount = 0;
			++mCount;

			if (mMetricCameraCount)
				mMetricCameraCount->Set(static_cast<double>(mCount));
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::Unregister(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			if (idx < 0)
			{
				DIA_LOG_WARNING("DiaCamera2D", "CameraRegistry2D::Unregister — camera not found");
				return;
			}
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::Unregister — camera not found");

			DeleteBehaviours(static_cast<unsigned int>(idx));

			// Patch active index if needed
			if (mActiveIndex == idx)
				mActiveIndex = -1;
			else if (mActiveIndex > idx)
				--mActiveIndex;

			// Swap with last to fill gap
			const unsigned int last = mCount - 1;
			if (static_cast<unsigned int>(idx) != last)
				mSlots[idx] = mSlots[last];

			mSlots[last].behaviourCount = 0;
			--mCount;

			if (mMetricCameraCount)
				mMetricCameraCount->Set(static_cast<double>(mCount));
		}

		////////////////////////////////////////////////////////////
		bool CameraRegistry2D::Has(Dia::Core::StringCRC id) const
		{
			return FindIndex(id) >= 0;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour* behaviour)
		{
			const int idx = FindIndex(cameraId);
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::AttachBehaviour — camera not found");
			DIA_ASSERT(behaviour != nullptr, "CameraRegistry2D::AttachBehaviour — null behaviour");

			CameraSlot& slot = mSlots[idx];
			DIA_ASSERT(slot.behaviourCount < kMaxBehaviours, "CameraRegistry2D::AttachBehaviour — behaviour capacity exceeded");
			slot.behaviours[slot.behaviourCount++] = behaviour;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId)
		{
			const int idx = FindIndex(cameraId);
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::DetachBehaviour — camera not found");

			CameraSlot& slot = mSlots[idx];
			for (unsigned int b = 0; b < slot.behaviourCount; ++b)
			{
				if (slot.behaviours[b]->GetTypeId() == behaviourTypeId)
				{
					delete slot.behaviours[b];
					// Shift remaining behaviours down
					for (unsigned int r = b; r < slot.behaviourCount - 1; ++r)
						slot.behaviours[r] = slot.behaviours[r + 1];
					slot.behaviours[--slot.behaviourCount] = nullptr;
					return;
				}
			}
			DIA_ASSERT(false, "CameraRegistry2D::DetachBehaviour — behaviour type not found");
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::SetActive(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::SetActive — camera not found");
			mActiveIndex = idx;
		}

		////////////////////////////////////////////////////////////
		Camera2D& CameraRegistry2D::GetActive()
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry2D::GetActive — no active camera set");
			return mSlots[mActiveIndex].camera;
		}

		////////////////////////////////////////////////////////////
		const Camera2D& CameraRegistry2D::GetActive() const
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry2D::GetActive — no active camera set");
			return mSlots[mActiveIndex].camera;
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC CameraRegistry2D::GetActiveId() const
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry2D::GetActiveId — no active camera set");
			return mSlots[mActiveIndex].id;
		}

		////////////////////////////////////////////////////////////
		Camera2D& CameraRegistry2D::Get(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::Get — camera not found");
			return mSlots[idx].camera;
		}

		////////////////////////////////////////////////////////////
		const Camera2D& CameraRegistry2D::Get(Dia::Core::StringCRC id) const
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry2D::Get — camera not found");
			return mSlots[idx].camera;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::UpdateAll(float dt)
		{
			DIA_TRACE_ZONE("camera2d.update_all", Dia::Observation::Trace::Category::kDiaGraphics);
			DIA_PROFILE_SCOPE("camera2d.update_all", Dia::Observation::Profile::Category::kDiaGraphics);

			unsigned int totalTicks = 0;
			for (unsigned int i = 0; i < mCount; ++i)
			{
				CameraSlot& slot = mSlots[i];
				for (unsigned int b = 0; b < slot.behaviourCount; ++b)
				{
					slot.behaviours[b]->Update(slot.camera, dt);
					++totalTicks;
				}
			}

			if (mMetricBehaviourTicks) mMetricBehaviourTicks->Inc(totalTicks);
		}

		////////////////////////////////////////////////////////////
		int CameraRegistry2D::FindIndex(Dia::Core::StringCRC id) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mSlots[i].id == id)
					return static_cast<int>(i);
			}
			return -1;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry2D::DeleteBehaviours(unsigned int slotIndex)
		{
			CameraSlot& slot = mSlots[slotIndex];
			for (unsigned int b = 0; b < slot.behaviourCount; ++b)
			{
				delete slot.behaviours[b];
				slot.behaviours[b] = nullptr;
			}
			slot.behaviourCount = 0;
		}

	} // namespace Camera2D
} // namespace Dia
