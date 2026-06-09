////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistry3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Registry/CameraRegistry3D.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		CameraRegistry3D::CameraRegistry3D()
		{
			for (unsigned int i = 0; i < kMaxCameras; ++i)
			{
				mSlots[i].behaviourCount = 0;
				for (unsigned int b = 0; b < kMaxBehaviours; ++b)
					mSlots[i].behaviours[b] = nullptr;
			}

			auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
			mMetricCameraCount    = reg.RegisterGauge(Dia::Core::StringCRC("dia.camera3d.count"));
			mMetricBehaviourTicks = reg.RegisterCounter(Dia::Core::StringCRC("dia.camera3d.behaviour_ticks"));
			mMetricActiveChanges  = reg.RegisterCounter(Dia::Core::StringCRC("dia.camera3d.active_changes"));
		}

		////////////////////////////////////////////////////////////
		CameraRegistry3D::~CameraRegistry3D()
		{
			for (unsigned int i = 0; i < mCount; ++i)
				DeleteBehaviours(i);
		}

		////////////////////////////////////////////////////////////
		bool CameraRegistry3D::Register(Dia::Core::StringCRC id, const Camera3D& camera)
		{
			if (Has(id))
			{
				DIA_LOG_WARNING("DiaCamera3D", "CameraRegistry3D::Register — duplicate camera id, ignoring");
				return false;
			}
			DIA_ASSERT(mCount < kMaxCameras, "CameraRegistry3D::Register — capacity exceeded");

			mSlots[mCount].id             = id;
			mSlots[mCount].camera         = camera;
			mSlots[mCount].behaviourCount = 0;
			++mCount;
			DIA_LOG_DEBUG("DiaCamera3D", "CameraRegistry3D::Register — camera registered");

			if (mMetricCameraCount)
				mMetricCameraCount->Set(static_cast<double>(mCount));

			return true;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry3D::Unregister(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			if (idx < 0)
			{
				DIA_LOG_WARNING("DiaCamera3D", "CameraRegistry3D::Unregister — camera not found");
				return;
			}

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
			DIA_LOG_DEBUG("DiaCamera3D", "CameraRegistry3D::Unregister — camera unregistered");

			if (mMetricCameraCount)
				mMetricCameraCount->Set(static_cast<double>(mCount));
		}

		////////////////////////////////////////////////////////////
		bool CameraRegistry3D::Has(Dia::Core::StringCRC id) const
		{
			return FindIndex(id) >= 0;
		}

		////////////////////////////////////////////////////////////
		bool CameraRegistry3D::AttachBehaviour(Dia::Core::StringCRC cameraId, ICameraBehaviour3D* behaviour)
		{
			const int idx = FindIndex(cameraId);
			if (idx < 0)
			{
				DIA_LOG_WARNING("DiaCamera3D", "CameraRegistry3D::AttachBehaviour — camera not found");
				return false;
			}

			DIA_ASSERT(behaviour != nullptr, "CameraRegistry3D::AttachBehaviour — null behaviour");

			CameraSlot& slot = mSlots[idx];
			if (slot.behaviourCount >= kMaxBehaviours)
			{
				DIA_LOG_WARNING("DiaCamera3D", "CameraRegistry3D::AttachBehaviour — behaviour capacity exceeded");
				return false;
			}

			slot.behaviours[slot.behaviourCount++] = behaviour;
			return true;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry3D::DetachBehaviour(Dia::Core::StringCRC cameraId, Dia::Core::StringCRC behaviourTypeId)
		{
			const int idx = FindIndex(cameraId);
			DIA_ASSERT(idx >= 0, "CameraRegistry3D::DetachBehaviour — camera not found");

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
			DIA_ASSERT(false, "CameraRegistry3D::DetachBehaviour — behaviour type not found");
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry3D::SetActive(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry3D::SetActive — camera not found");
			mActiveIndex = idx;
			if (mMetricActiveChanges) mMetricActiveChanges->Inc(1);
			DIA_LOG_DEBUG("DiaCamera3D", "CameraRegistry3D::SetActive — active camera changed");
		}

		////////////////////////////////////////////////////////////
		Camera3D& CameraRegistry3D::GetActive()
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry3D::GetActive — no active camera set");
			return mSlots[mActiveIndex].camera;
		}

		////////////////////////////////////////////////////////////
		const Camera3D& CameraRegistry3D::GetActive() const
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry3D::GetActive — no active camera set");
			return mSlots[mActiveIndex].camera;
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC CameraRegistry3D::GetActiveId() const
		{
			DIA_ASSERT(mActiveIndex >= 0, "CameraRegistry3D::GetActiveId — no active camera set");
			return mSlots[mActiveIndex].id;
		}

		////////////////////////////////////////////////////////////
		Camera3D& CameraRegistry3D::Get(Dia::Core::StringCRC id)
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry3D::Get — camera not found");
			return mSlots[idx].camera;
		}

		////////////////////////////////////////////////////////////
		const Camera3D& CameraRegistry3D::Get(Dia::Core::StringCRC id) const
		{
			const int idx = FindIndex(id);
			DIA_ASSERT(idx >= 0, "CameraRegistry3D::Get — camera not found");
			return mSlots[idx].camera;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry3D::UpdateAll(float dt)
		{
			DIA_TRACE_ZONE("camera3d.update_all", Dia::Observation::Trace::Category::kDiaGraphics);
			DIA_PROFILE_SCOPE("camera3d.update_all", Dia::Observation::Profile::Category::kDiaGraphics);

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
		int CameraRegistry3D::FindIndex(Dia::Core::StringCRC id) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mSlots[i].id == id)
					return static_cast<int>(i);
			}
			return -1;
		}

		////////////////////////////////////////////////////////////
		void CameraRegistry3D::DeleteBehaviours(unsigned int slotIndex)
		{
			CameraSlot& slot = mSlots[slotIndex];
			for (unsigned int b = 0; b < slot.behaviourCount; ++b)
			{
				delete slot.behaviours[b];
				slot.behaviours[b] = nullptr;
			}
			slot.behaviourCount = 0;
		}

	} // namespace Camera3D
} // namespace Dia
