////////////////////////////////////////////////////////////////////////////////
// Filename: ScreenShake3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/ScreenShake3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Core/Angle.h>
#include <cmath>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		ScreenShake3D::ScreenShake3D(float maxPositionOffset, float maxAngleOffset, float traumaDecay)
			: mMaxPositionOffset(maxPositionOffset)
			, mMaxAngleOffset(maxAngleOffset)
			, mTraumaDecay(traumaDecay)
		{
			auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
			mMetricTrauma = reg.RegisterGauge(Dia::Core::StringCRC("dia.camera3d.screenshake_trauma"));
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC ScreenShake3D::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void ScreenShake3D::Trigger(float trauma)
		{
			mTrauma += trauma;
			if (mTrauma > 1.0f) mTrauma = 1.0f;
		}

		////////////////////////////////////////////////////////////
		void ScreenShake3D::Update(Camera3D& camera, float dt)
		{
			if (mTrauma <= 0.0f) return;

			mTime += dt;

			const float magnitude = mTrauma * mTrauma;

			// Additive position shake — incommensurate sin frequencies
			const float posOffset = magnitude * mMaxPositionOffset;
			camera.position = Dia::Maths::Vector3D(
				camera.position.x + posOffset * sinf(mTime * 37.0f),
				camera.position.y + posOffset * sinf(mTime * 53.0f),
				camera.position.z + posOffset * sinf(mTime * 29.0f));

			// Additive orientation shake — small yaw/pitch rotation
			const float angleOffset = magnitude * mMaxAngleOffset;
			const Dia::Maths::Quaternion shakeRot =
				Dia::Maths::Quaternion::FromEuler(
					Dia::Maths::Angle::FromRadians(angleOffset * sinf(mTime * 41.0f)),  // yaw
					Dia::Maths::Angle::FromRadians(angleOffset * sinf(mTime * 67.0f)),  // pitch
					Dia::Maths::Angle::FromRadians(0.0f));                               // roll
			camera.orientation = camera.orientation * shakeRot;

			mTrauma -= mTraumaDecay * dt;
			if (mTrauma < 0.0f) { mTrauma = 0.0f; mTime = 0.0f; }
			if (mMetricTrauma) mMetricTrauma->Set(static_cast<double>(mTrauma));
		}

	} // namespace Camera3D
} // namespace Dia
