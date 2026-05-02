#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace IK2D
	{
		static const unsigned int kMaxJointLimits = 32;

		struct JointLimitDef
		{
			float minAngle = -Dia::Maths::PI;
			float maxAngle =  Dia::Maths::PI;
			bool  enabled  = false;
		};

		struct IKChainDef
		{
			Dia::Core::StringCRC id;
			Dia::Core::StringCRC startBoneId;
			Dia::Core::StringCRC endBoneId;
			float reachWeight   = 1.0f;
			int   maxIterations = 20;
			float tolerance     = 0.001f;
			Dia::Core::Containers::DynamicArrayC<JointLimitDef, kMaxJointLimits> jointLimits;
		};

		struct PoleVector
		{
			Dia::Maths::Vector2D direction = Dia::Maths::Vector2D(1.0f, 0.0f);
			float weight = 1.0f;
		};
	}
}
