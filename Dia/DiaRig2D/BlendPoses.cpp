#include "BlendPoses.h"

#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia
{
	namespace Rig2D
	{
		namespace
		{
			float NormalizeAngle(float angle)
			{
				while (angle > 3.14159265f)
					angle -= 2.0f * 3.14159265f;
				while (angle < -3.14159265f)
					angle += 2.0f * 3.14159265f;
				return angle;
			}

			float LerpAngle(float a, float b, float t)
			{
				float diff = NormalizeAngle(b - a);
				return a + diff * t;
			}
		}

		void BlendPoses(const Pose& a, const Pose& b, float t, Pose& outResult)
		{
			DIA_ASSERT(a.GetBoneCount() == b.GetBoneCount(),
				"BlendPoses: bone count mismatch (%d vs %d)", a.GetBoneCount(), b.GetBoneCount());
			DIA_ASSERT(a.GetBoneCount() == outResult.GetBoneCount(),
				"BlendPoses: output bone count %d does not match input %d", outResult.GetBoneCount(), a.GetBoneCount());

			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;

			float oneMinusT = 1.0f - t;

			for (int i = 0; i < a.GetBoneCount(); ++i)
			{
				const BoneTransform& btA = a.GetLocalTransform(i);
				const BoneTransform& btB = b.GetLocalTransform(i);
				BoneTransform& btOut = outResult.GetLocalTransform(i);

				btOut.position.Set(
					btA.position.X() * oneMinusT + btB.position.X() * t,
					btA.position.Y() * oneMinusT + btB.position.Y() * t
				);

				btOut.rotation = LerpAngle(btA.rotation, btB.rotation, t);

				btOut.scale.Set(
					btA.scale.X() * oneMinusT + btB.scale.X() * t,
					btA.scale.Y() * oneMinusT + btB.scale.Y() * t
				);
			}
		}
	}
}
