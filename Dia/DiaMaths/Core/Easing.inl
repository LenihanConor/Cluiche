#include "DiaMaths/Core/MathsDefines.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"

namespace Dia
{
	namespace Maths
	{
		namespace Easing
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Implementation
			//---------------------------------------------------------------------------------------------------------------------------------

			//-----------------------------------------------------------------------------
			// Linear
			inline float Linear(float t)
			{
				return t;
			}

			//-----------------------------------------------------------------------------
			// Quadratic
			inline float EaseInQuad(float t)
			{
				return t * t;
			}

			inline float EaseOutQuad(float t)
			{
				return t * (2.0f - t);
			}

			inline float EaseInOutQuad(float t)
			{
				if (t < 0.5f)
					return 2.0f * t * t;
				else
				{
					t = t * 2.0f - 1.0f;
					return -0.5f * (t * (t - 2.0f) - 1.0f);
				}
			}

			//-----------------------------------------------------------------------------
			// Cubic
			inline float EaseInCubic(float t)
			{
				return t * t * t;
			}

			inline float EaseOutCubic(float t)
			{
				t -= 1.0f;
				return t * t * t + 1.0f;
			}

			inline float EaseInOutCubic(float t)
			{
				if (t < 0.5f)
					return 4.0f * t * t * t;
				else
				{
					t = t * 2.0f - 2.0f;
					return 0.5f * t * t * t + 1.0f;
				}
			}

			//-----------------------------------------------------------------------------
			// Quartic
			inline float EaseInQuart(float t)
			{
				return t * t * t * t;
			}

			inline float EaseOutQuart(float t)
			{
				t -= 1.0f;
				return 1.0f - t * t * t * t;
			}

			inline float EaseInOutQuart(float t)
			{
				if (t < 0.5f)
					return 8.0f * t * t * t * t;
				else
				{
					t = t * 2.0f - 2.0f;
					return 1.0f - 0.5f * t * t * t * t;
				}
			}

			//-----------------------------------------------------------------------------
			// Quintic
			inline float EaseInQuint(float t)
			{
				return t * t * t * t * t;
			}

			inline float EaseOutQuint(float t)
			{
				t -= 1.0f;
				return t * t * t * t * t + 1.0f;
			}

			inline float EaseInOutQuint(float t)
			{
				if (t < 0.5f)
					return 16.0f * t * t * t * t * t;
				else
				{
					t = t * 2.0f - 2.0f;
					return 0.5f * t * t * t * t * t + 1.0f;
				}
			}

			//-----------------------------------------------------------------------------
			// Sine
			inline float EaseInSine(float t)
			{
				return 1.0f - Dia::Maths::Cos(t * PI_HALF);
			}

			inline float EaseOutSine(float t)
			{
				return Dia::Maths::Sin(t * PI_HALF);
			}

			inline float EaseInOutSine(float t)
			{
				return 0.5f * (1.0f - Dia::Maths::Cos(t * PI));
			}

			//-----------------------------------------------------------------------------
			// Exponential
			inline float EaseInExpo(float t)
			{
				if (t <= 0.0f)
					return 0.0f;
				return Dia::Maths::Float::FPower(2.0f, 10.0f * (t - 1.0f));
			}

			inline float EaseOutExpo(float t)
			{
				if (t >= 1.0f)
					return 1.0f;
				return 1.0f - Dia::Maths::Float::FPower(2.0f, -10.0f * t);
			}

			inline float EaseInOutExpo(float t)
			{
				if (t <= 0.0f)
					return 0.0f;
				if (t >= 1.0f)
					return 1.0f;

				if (t < 0.5f)
					return 0.5f * Dia::Maths::Float::FPower(2.0f, (20.0f * t) - 10.0f);
				else
					return 1.0f - 0.5f * Dia::Maths::Float::FPower(2.0f, -20.0f * t + 10.0f);
			}

			//-----------------------------------------------------------------------------
			// Circular
			inline float EaseInCirc(float t)
			{
				return 1.0f - Dia::Maths::Float::FSquareRoot(1.0f - t * t);
			}

			inline float EaseOutCirc(float t)
			{
				t -= 1.0f;
				return Dia::Maths::Float::FSquareRoot(1.0f - t * t);
			}

			inline float EaseInOutCirc(float t)
			{
				if (t < 0.5f)
				{
					t = t * 2.0f;
					return 0.5f * (1.0f - Dia::Maths::Float::FSquareRoot(1.0f - t * t));
				}
				else
				{
					t = t * 2.0f - 2.0f;
					return 0.5f * (Dia::Maths::Float::FSquareRoot(1.0f - t * t) + 1.0f);
				}
			}

			//-----------------------------------------------------------------------------
			// Back (overshoot)
			inline float EaseInBack(float t)
			{
				const float s = 1.70158f;
				return t * t * ((s + 1.0f) * t - s);
			}

			inline float EaseOutBack(float t)
			{
				const float s = 1.70158f;
				t -= 1.0f;
				return t * t * ((s + 1.0f) * t + s) + 1.0f;
			}

			inline float EaseInOutBack(float t)
			{
				const float s = 1.70158f * 1.525f;

				if (t < 0.5f)
				{
					t = t * 2.0f;
					return 0.5f * (t * t * ((s + 1.0f) * t - s));
				}
				else
				{
					t = t * 2.0f - 2.0f;
					return 0.5f * (t * t * ((s + 1.0f) * t + s) + 2.0f);
				}
			}

			//-----------------------------------------------------------------------------
			// Elastic (spring)
			inline float EaseInElastic(float t)
			{
				if (t <= 0.0f)
					return 0.0f;
				if (t >= 1.0f)
					return 1.0f;

				const float p = 0.3f;
				const float s = p / 4.0f;

				t -= 1.0f;
				return -(Dia::Maths::Float::FPower(2.0f, 10.0f * t) * Dia::Maths::Sin((t - s) * PI_2 / p));
			}

			inline float EaseOutElastic(float t)
			{
				if (t <= 0.0f)
					return 0.0f;
				if (t >= 1.0f)
					return 1.0f;

				const float p = 0.3f;
				const float s = p / 4.0f;

				return Dia::Maths::Float::FPower(2.0f, -10.0f * t) * Dia::Maths::Sin((t - s) * PI_2 / p) + 1.0f;
			}

			inline float EaseInOutElastic(float t)
			{
				if (t <= 0.0f)
					return 0.0f;
				if (t >= 1.0f)
					return 1.0f;

				const float p = 0.45f;
				const float s = p / 4.0f;

				if (t < 0.5f)
				{
					t = t * 2.0f - 1.0f;
					return -0.5f * (Dia::Maths::Float::FPower(2.0f, 10.0f * t) * Dia::Maths::Sin((t - s) * PI_2 / p));
				}
				else
				{
					t = t * 2.0f - 1.0f;
					return 0.5f * Dia::Maths::Float::FPower(2.0f, -10.0f * t) * Dia::Maths::Sin((t - s) * PI_2 / p) + 1.0f;
				}
			}

			//-----------------------------------------------------------------------------
			// Bounce
			inline float EaseOutBounce(float t)
			{
				const float n1 = 7.5625f;
				const float d1 = 2.75f;

				if (t < 1.0f / d1)
				{
					return n1 * t * t;
				}
				else if (t < 2.0f / d1)
				{
					t -= 1.5f / d1;
					return n1 * t * t + 0.75f;
				}
				else if (t < 2.5f / d1)
				{
					t -= 2.25f / d1;
					return n1 * t * t + 0.9375f;
				}
				else
				{
					t -= 2.625f / d1;
					return n1 * t * t + 0.984375f;
				}
			}

			inline float EaseInBounce(float t)
			{
				return 1.0f - EaseOutBounce(1.0f - t);
			}

			inline float EaseInOutBounce(float t)
			{
				if (t < 0.5f)
					return 0.5f * EaseInBounce(t * 2.0f);
				else
					return 0.5f * EaseOutBounce(t * 2.0f - 1.0f) + 0.5f;
			}
		}
	}
}
