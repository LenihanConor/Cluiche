#pragma once

#include "DiaRig2D/Pose.h"

namespace Dia
{
	namespace Rig2D
	{
		namespace Testing
		{
			bool BoneTransformsAreEqual(const BoneTransform& a, const BoneTransform& b, float tolerance = 0.001f);
			bool PosesAreEqual(const Pose& a, const Pose& b, float tolerance = 0.001f);
		}
	}
}
