#pragma once

namespace Dia
{
	namespace Maths
	{
		// Mathematical constants
		// Using constexpr (C++11) to avoid ODR violations
		// These are evaluated at compile-time and create no storage
		constexpr float PI   = 3.1415926535897932384626433832795f;
		constexpr float PI_2	= (PI * 2.0f);
		constexpr float PI_HALF	= (PI / 2.0f);
		constexpr float PI_ONE_OVER = (1.0f / PI);
		constexpr float PI_HALF_ONE_OVER = (2.0f / PI);

		constexpr float DEGREETORADII = 0.0174532925199f;
		constexpr float RADIITODEGREE = 57.295779513082f;

		constexpr float MAX_RADINS = PI_2;
		constexpr float MIN_RADIANS = 0;

		constexpr float ONE_OVER_SQRT_2	= 0.70710678118654746f;

		constexpr float FLOAT_EPSILON = 0.0001f;
	}
}