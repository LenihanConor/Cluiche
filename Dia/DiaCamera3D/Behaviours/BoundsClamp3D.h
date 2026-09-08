////////////////////////////////////////////////////////////////////////////////
// Filename: BoundsClamp3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "DiaCamera3D/Registry/ICameraBehaviour3D.h"
#include <DiaGeometry3D/Shapes/AABB.h>

namespace Dia
{
	namespace Camera3D
	{
		// Clamps camera position within an AABB. No-op if AABB has zero volume.
		class BoundsClamp3D : public ICameraBehaviour3D
		{
		public:
			BoundsClamp3D() = default;
			explicit BoundsClamp3D(const Dia::Geometry3D::AABB& bounds);

			Dia::Core::StringCRC GetTypeId() const override;
			void Update(Camera3D& camera, float dt) override;

			void SetBounds(const Dia::Geometry3D::AABB& bounds) { mBounds = bounds; }

			static constexpr const char* kTypeIdStr = "BoundsClamp3D";

		private:
			Dia::Geometry3D::AABB mBounds;
		};

	} // namespace Camera3D
} // namespace Dia
