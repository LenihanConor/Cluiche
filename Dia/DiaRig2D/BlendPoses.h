#pragma once

#include "Pose.h"

namespace Dia
{
	namespace Rig2D
	{
		void BlendPoses(const Pose& a, const Pose& b, float t, Pose& outResult);
	}
}
