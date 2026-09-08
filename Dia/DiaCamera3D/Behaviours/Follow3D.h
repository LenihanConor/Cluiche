////////////////////////////////////////////////////////////////////////////////
// Filename: Follow3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaMaths/Vector/Vector3D.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		/// \brief Hard-follow behaviour: snaps the camera to target + offset
		///        every frame, with no lag.
		////////////////////////////////////////////////////////////
		class Follow3D : public ICameraBehaviour3D
		{
		public:
			explicit Follow3D(Dia::Maths::Vector3D offset = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f));

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera3D& camera, float dt) override;

			void SetTarget(const Dia::Maths::Vector3D& target) { mTarget = target; mHasTarget = true; }
			void SetOffset(const Dia::Maths::Vector3D& offset) { mOffset = offset; }

			static constexpr const char* kTypeIdStr = "Follow3D";

		private:
			Dia::Maths::Vector3D mTarget;
			Dia::Maths::Vector3D mOffset;
			bool                 mHasTarget = false;
		};

	} // namespace Camera3D
} // namespace Dia
