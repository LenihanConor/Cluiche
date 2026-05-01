#pragma once

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Linear Interpolation Functions
		//
		// These functions provide smooth transitions between values, essential for animation,
		// movement, camera systems, and any time-based value changes in games.
		//---------------------------------------------------------------------------------------------------------------------------------

		// Linear interpolation between a and b by t (clamped to 0-1)
		// Returns: a when t=0, b when t=1, and values in between for 0<t<1
		// Parameters:
		//   a - Start value
		//   b - End value
		//   t - Interpolation factor (automatically clamped to 0-1)
		// Usage: Lerp(0.0f, 100.0f, 0.5f) = 50.0f
		// NOTE: Template T must support arithmetic operators (+, -, *)
		//       For Vector2D, use Vector2D::Lerp() instead
		template <class T> T Lerp(const T& a, const T& b, float t);

		// Linear interpolation between a and b by t (not clamped)
		// Allows extrapolation beyond the a-b range when t < 0 or t > 1
		// Parameters:
		//   a - Start value
		//   b - End value
		//   t - Interpolation factor (can be any value)
		// Usage: LerpUnclamped(0.0f, 100.0f, 1.5f) = 150.0f
		// NOTE: Template T must support arithmetic operators (+, -, *)
		//       For Vector2D, use Vector2D::LerpUnclamped() instead
		template <class T> T LerpUnclamped(const T& a, const T& b, float t);

		// Inverse lerp - calculates what t value produces 'value' between a and b
		// Returns: 0 if a == b (to avoid division by zero)
		// Parameters:
		//   a     - Start value
		//   b     - End value
		//   value - The value to find the t for
		// Usage: InverseLerp(0.0f, 100.0f, 25.0f) = 0.25f
		// This is useful for converting a value to a normalized 0-1 range
		// NOTE: Template T must be a SCALAR type (float, int, etc.)
		//       This function uses (diff * diff) for squared distance
		//       which only works correctly for scalar types
		template <class T> float InverseLerp(const T& a, const T& b, const T& value);

		// Smooth interpolation between 0 and 1 using smoothstep (3t^2 - 2t^3)
		// Provides smooth acceleration and deceleration at the start and end
		// Parameters:
		//   t - Input value (clamped to 0-1)
		// Returns: Smoothed value between 0 and 1
		// Note: Has zero first derivative at t=0 and t=1 (smooth start/stop)
		float SmoothStep(float t);

		// Smoother interpolation using Ken Perlin's improved smoothstep (6t^5 - 15t^4 + 10t^3)
		// Even smoother than SmoothStep, with zero first AND second derivatives at endpoints
		// Parameters:
		//   t - Input value (clamped to 0-1)
		// Returns: Extra-smoothed value between 0 and 1
		// Note: Better for high-quality animations where very smooth motion is needed
		float SmootherStep(float t);

		// Smooth interpolation between a and b using smoothstep
		// Combines Lerp with SmoothStep for smooth ease-in/ease-out motion
		// Parameters:
		//   a - Start value
		//   b - End value
		//   t - Interpolation factor (clamped to 0-1, then smoothed)
		template <class T> T SmoothLerp(const T& a, const T& b, float t);

		// Smooth interpolation between a and b using smootherstep
		// Combines Lerp with SmootherStep for extra-smooth motion
		// Parameters:
		//   a - Start value
		//   b - End value
		//   t - Interpolation factor (clamped to 0-1, then smoothed)
		template <class T> T SmootherLerp(const T& a, const T& b, float t);

		//---------------------------------------------------------------------------------------------------------------------------------
		// Movement Functions
		//
		// Functions for moving values toward targets with constraints, useful for
		// following cameras, smooth character movement, and other game mechanics.
		//---------------------------------------------------------------------------------------------------------------------------------

		// Moves current toward target by maxDelta (does not overshoot)
		// If the distance to target is less than maxDelta, returns target exactly
		// Parameters:
		//   current  - Current value
		//   target   - Target value to move toward
		//   maxDelta - Maximum distance to move (must be positive)
		// Usage: Moving a camera to follow a player with limited speed
		// NOTE: Template T must be a SCALAR type (float, int, etc.)
		//       For Vector2D, use Vector2D::MoveTowards() instead
		//       This function uses (diff * diff) which only works for scalars
		template <class T> T MoveTowards(const T& current, const T& target, float maxDelta);

		// Remaps value from one range to another
		// Converts a value from [fromMin, fromMax] to [toMin, toMax]
		// Parameters:
		//   value   - The value to remap
		//   fromMin - Minimum of input range
		//   fromMax - Maximum of input range
		//   toMin   - Minimum of output range
		//   toMax   - Maximum of output range
		// Example: Remap(0.5f, 0.0f, 1.0f, 10.0f, 20.0f) = 15.0f
		// Usage: Converting joystick input (-1 to 1) to screen coordinates
		float Remap(float value, float fromMin, float fromMax, float toMin, float toMax);

		// Remaps value from one range to another (clamped to output range)
		// Same as Remap but ensures output is within [toMin, toMax]
		// Parameters: Same as Remap
		// Usage: Converting input values that might go out of range
		float RemapClamped(float value, float fromMin, float fromMax, float toMin, float toMax);
	}
}

#include "DiaMaths/Core/Interpolation.inl"
