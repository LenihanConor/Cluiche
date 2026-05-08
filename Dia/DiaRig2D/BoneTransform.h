#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Rig2D
	{
		struct BoneTransform
		{
			Dia::Maths::Vector2D	position = Dia::Maths::Vector2D(0.0f, 0.0f);
			float					rotation = 0.0f;
			Dia::Maths::Vector2D	scale = Dia::Maths::Vector2D(1.0f, 1.0f);
		};
	}
}
