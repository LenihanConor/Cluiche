////////////////////////////////////////////////////////////////////////////////
// Filename: SmoothDamp3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaMaths/Vector/Vector3D.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		/// \brief Critically-damped spring behaviour: smoothly moves the
		///        camera toward its target with no overshoot.
		///
		/// Algorithm: Game Programming Gems vol.4 spring approximation,
		/// extended to all three axes independently.
		////////////////////////////////////////////////////////////
		class SmoothDamp3D : public ICameraBehaviour3D
		{
		public:
			explicit SmoothDamp3D(float smoothTime = 0.15f);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera3D& camera, float dt) override;

			void SetTarget(const Dia::Maths::Vector3D& target) { mTarget = target; mHasTarget = true; }
			void SetSmoothTime(float t) { mSmoothTime = t; }

			static constexpr const char* kTypeIdStr = "SmoothDamp3D";

		private:
			Dia::Maths::Vector3D mTarget;
			Dia::Maths::Vector3D mVelocity;
			float                mSmoothTime = 0.15f;
			bool                 mHasTarget  = false;
		};

	} // namespace Camera3D
} // namespace Dia
