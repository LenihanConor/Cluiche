#pragma once

#include "DiaRig2D/Skeleton.h"

namespace Dia
{
	namespace Rig2D
	{
		namespace Testing
		{
			SkeletonDef MakeSimpleChain(int boneCount);
			SkeletonDef MakeHumanoid();
			SkeletonDef MakeBranching();
		}
	}
}
