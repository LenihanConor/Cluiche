#pragma once

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Easing Functions
		//
		// Easing functions modify the rate of change over time, creating smooth, natural motion.
		// Essential for polished game feel: UI animations, camera movement, character actions, etc.
		//
		// INPUT: t in range [0, 1] (typically time normalized to animation duration)
		// OUTPUT: Eased value, typically in [0, 1] but may exceed for overshoot effects
		//
		// TYPES:
		//   In:    Slow start, fast end (acceleration)
		//   Out:   Fast start, slow end (deceleration)
		//   InOut: Slow start, fast middle, slow end (smooth both ends)
		//
		// USAGE EXAMPLE:
		//   float t = currentTime / duration;           // Normalize time to 0-1
		//   float eased = Easing::EaseOutCubic(t);      // Apply easing
		//   float value = Lerp(startValue, endValue, eased); // Interpolate
		//
		// CHOOSING AN EASING:
		//   - Quad/Cubic:    Subtle, natural motion (most common)
		//   - Expo/Circ:     Dramatic, pronounced motion
		//   - Back:          Overshoots target (anticipation/follow-through)
		//   - Elastic:       Bouncy spring effect
		//   - Bounce:        Ball bouncing effect
		//
		// Based on Robert Penner's easing equations - industry standard
		//---------------------------------------------------------------------------------------------------------------------------------

		namespace Easing
		{
			// Linear (no easing) - constant speed
			// Use for: Mechanical motion, when you want consistent speed
			float Linear(float t);

			// Quadratic easing (t^2) - subtle acceleration/deceleration
			// Use for: Gentle UI animations, smooth character movement
			float EaseInQuad(float t);   // Accelerating from zero
			float EaseOutQuad(float t);  // Decelerating to zero (most common!)
			float EaseInOutQuad(float t);// Smooth start and end

			// Cubic easing (t^3) - moderate acceleration/deceleration
			// Use for: More pronounced motion than Quad, still natural feeling
			float EaseInCubic(float t);
			float EaseOutCubic(float t);
			float EaseInOutCubic(float t);

			// Quartic easing (t^4) - strong acceleration/deceleration
			// Use for: Dramatic motion, powerful impacts
			float EaseInQuart(float t);
			float EaseOutQuart(float t);
			float EaseInOutQuart(float t);

			// Quintic easing (t^5) - very strong acceleration/deceleration
			// Use for: Extreme motion, cinematic effects
			float EaseInQuint(float t);
			float EaseOutQuint(float t);
			float EaseInOutQuint(float t);

			// Sine easing - smooth, gentle curve
			// Use for: Natural, organic motion
			float EaseInSine(float t);
			float EaseOutSine(float t);
			float EaseInOutSine(float t);

			// Exponential easing - very dramatic, starts slow then very fast (or vice versa)
			// Use for: Zoom effects, explosive motion
			float EaseInExpo(float t);
			float EaseOutExpo(float t);
			float EaseInOutExpo(float t);

			// Circular easing - based on circle arc, moderate curve
			// Use for: Smooth transitions, speed changes
			float EaseInCirc(float t);
			float EaseOutCirc(float t);
			float EaseInOutCirc(float t);

			// Back easing - overshoots target then returns (anticipation/follow-through)
			// Use for: UI elements "pulling back" before moving, cartoon-style motion
			float EaseInBack(float t);   // Pulls back before moving forward
			float EaseOutBack(float t);  // Overshoots then settles (most common!)
			float EaseInOutBack(float t);// Both effects

			// Elastic easing - spring/rubber band effect, oscillates around target
			// Use for: Bouncy UI, spring-loaded objects, exaggerated cartoon motion
			float EaseInElastic(float t);
			float EaseOutElastic(float t);  // Most common - settles with bounce
			float EaseInOutElastic(float t);

			// Bounce easing - ball bouncing effect, multiple diminishing bounces
			// Use for: Literally bouncing objects, playful UI
			float EaseInBounce(float t);
			float EaseOutBounce(float t);  // Most common - bounces at end
			float EaseInOutBounce(float t);
		}
	}
}

#include "DiaMaths/Core/Easing.inl"
