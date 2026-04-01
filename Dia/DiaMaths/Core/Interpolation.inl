namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Implementation
		//---------------------------------------------------------------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		// Linear interpolation between a and b by t (clamped to 0-1)
		// Formula: result = a + (b - a) * t
		// This is mathematically equivalent to: result = a * (1 - t) + b * t
		// but slightly more efficient as it only requires one multiplication
		template <class T> inline T Lerp(const T& a, const T& b, float t)
		{
			// Clamp t to valid range to prevent extrapolation
			t = Clamp(t, 0.0f, 1.0f);
			return a + (b - a) * t;
		}

		//-----------------------------------------------------------------------------
		// Linear interpolation between a and b by t (not clamped)
		// Allows extrapolation: if t > 1, result goes beyond b
		//                       if t < 0, result goes before a
		template <class T> inline T LerpUnclamped(const T& a, const T& b, float t)
		{
			return a + (b - a) * t;
		}

		//-----------------------------------------------------------------------------
		// Inverse lerp - calculates what t produces 'value' between a and b
		// Solves: value = a + (b - a) * t  for t
		// Result: t = (value - a) / (b - a)
		// Returns 0 if a == b to avoid division by zero
		template <class T> inline float InverseLerp(const T& a, const T& b, const T& value)
		{
			T diff = b - a;

			// Check if a and b are effectively equal (within epsilon)
			// We square both sides to avoid expensive sqrt for magnitude comparison
			if (diff * diff < static_cast<T>(FLOAT_EPSILON * FLOAT_EPSILON))
			{
				return 0.0f;  // Undefined, return 0 as safe default
			}

			// Standard inverse lerp formula
			return static_cast<float>((value - a) / diff);
		}

		//-----------------------------------------------------------------------------
		// Smooth interpolation between 0 and 1 using smoothstep (3t^2 - 2t^3)
		// This classic smoothstep function provides a smooth S-curve
		// Key properties:
		//   - f(0) = 0, f(1) = 1
		//   - f'(0) = 0, f'(1) = 0 (zero velocity at endpoints)
		//   - Smooth acceleration at start, smooth deceleration at end
		// Factored form: t * t * (3 - 2*t) is more efficient than 3t^2 - 2t^3
		inline float SmoothStep(float t)
		{
			t = Clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		//-----------------------------------------------------------------------------
		// Smoother interpolation using Ken Perlin's improved smoothstep (6t^5 - 15t^4 + 10t^3)
		// Even smoother than classic smoothstep
		// Key properties:
		//   - f(0) = 0, f(1) = 1
		//   - f'(0) = 0, f'(1) = 0 (zero first derivative at endpoints)
		//   - f''(0) = 0, f''(1) = 0 (zero second derivative at endpoints)
		// This means both velocity AND acceleration are zero at the start/end
		// Result: extremely smooth motion, no visible "jerk" at boundaries
		inline float SmootherStep(float t)
		{
			t = Clamp(t, 0.0f, 1.0f);
			// Factored efficiently: t^3 * (6t^2 - 15t + 10)
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		}

		//-----------------------------------------------------------------------------
		// Smooth interpolation between a and b using smoothstep
		template <class T> inline T SmoothLerp(const T& a, const T& b, float t)
		{
			float smoothT = SmoothStep(t);
			return LerpUnclamped(a, b, smoothT);
		}

		//-----------------------------------------------------------------------------
		// Smooth interpolation between a and b using smootherstep
		template <class T> inline T SmootherLerp(const T& a, const T& b, float t)
		{
			float smoothT = SmootherStep(t);
			return LerpUnclamped(a, b, smoothT);
		}

		//-----------------------------------------------------------------------------
		// Moves current toward target by maxDelta (does not overshoot)
		// This is useful for creating speed-limited following behavior
		// Example: Camera following player with max speed
		template <class T> inline T MoveTowards(const T& current, const T& target, float maxDelta)
		{
			T diff = target - current;
			float sqrDist = diff * diff;  // Squared distance (avoids expensive sqrt)

			// If we're already at target or closer than maxDelta, just return target
			// This ensures we don't overshoot and oscillate around the target
			if (sqrDist <= maxDelta * maxDelta || sqrDist < FLOAT_EPSILON)
			{
				return target;
			}

			// Move maxDelta distance toward target
			// We need actual distance for normalization, so sqrt here
			// Formula: current + direction * distance
			return current + diff * (maxDelta / sqrtf(sqrDist));
		}

		//-----------------------------------------------------------------------------
		// Remaps value from one range to another
		// Two-step process:
		//   1. Convert input value to 0-1 range using InverseLerp
		//   2. Map that 0-1 value to output range using Lerp
		// Example: Remap(5, 0, 10, 100, 200) = 150
		//   Step 1: InverseLerp(0, 10, 5) = 0.5
		//   Step 2: Lerp(100, 200, 0.5) = 150
		inline float Remap(float value, float fromMin, float fromMax, float toMin, float toMax)
		{
			float t = InverseLerp(fromMin, fromMax, value);
			return LerpUnclamped(toMin, toMax, t);
		}

		//-----------------------------------------------------------------------------
		// Remaps value from one range to another (clamped to output range)
		// Same as Remap but ensures result is within [toMin, toMax]
		// Useful when input might go outside expected range
		inline float RemapClamped(float value, float fromMin, float fromMax, float toMin, float toMax)
		{
			float t = InverseLerp(fromMin, fromMax, value);
			t = Clamp(t, 0.0f, 1.0f);  // Clamp normalized value
			return LerpUnclamped(toMin, toMax, t);
		}
	}
}
