////////////////////////////////////////////////////////////////////////////////
// Filename: PointLight2D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <stdint.h>

namespace Dia
{
	namespace Lighting2D
	{
		////////////////////////////////////////////////////////////
		/// \brief 2D point light value type.
		///
		/// Trivially copyable — no heap allocation, no virtual functions.
		/// layerMask is a uint32_t bitmask: bit N set means the light
		/// affects layer N. Resolution of layer names to bit indices is
		/// the caller's responsibility (typically DiaScene2D's LayerTable).
		////////////////////////////////////////////////////////////
		struct PointLight2D
		{
			Dia::Maths::Vector2D position  = {0.0f, 0.0f};
			float                radius    = 100.0f;
			float                colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // RGBA
			float                intensity = 1.0f;
			uint32_t             layerMask = 0xFFFFFFFF;  // affects all layers by default
			bool                 enabled   = true;
		};

	} // namespace Lighting2D
} // namespace Dia
