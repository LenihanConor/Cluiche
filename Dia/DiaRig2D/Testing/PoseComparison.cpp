#include "PoseComparison.h"

#include <cmath>

namespace Dia
{
	namespace Rig2D
	{
		namespace Testing
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
			}

			bool BoneTransformsAreEqual(const BoneTransform& a, const BoneTransform& b, float tolerance)
			{
				if (fabsf(a.position.X() - b.position.X()) > tolerance) return false;
				if (fabsf(a.position.Y() - b.position.Y()) > tolerance) return false;

				float rotDiff = NormalizeAngle(a.rotation - b.rotation);
				if (fabsf(rotDiff) > tolerance) return false;

				if (fabsf(a.scale.X() - b.scale.X()) > tolerance) return false;
				if (fabsf(a.scale.Y() - b.scale.Y()) > tolerance) return false;

				return true;
			}

			bool PosesAreEqual(const Pose& a, const Pose& b, float tolerance)
			{
				if (a.GetBoneCount() != b.GetBoneCount())
					return false;

				for (int i = 0; i < a.GetBoneCount(); ++i)
				{
					if (!BoneTransformsAreEqual(a.GetLocalTransform(i), b.GetLocalTransform(i), tolerance))
						return false;
				}

				return true;
			}
		}
	}
}
