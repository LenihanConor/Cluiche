#pragma once

#include <DiaGeometry2D/Shapes/AARect.h>

namespace Dia
{
	namespace Geometry2D
	{
		// Serializable config for SpatialGrid<T>::Def (type-independent).
		struct SpatialGridConfig
		{
			AARect worldBounds;
			float  cellSize = 1.0f;
		};

		// Serializable config for HexGrid<T>::Def (type-independent).
		struct HexGridConfig
		{
			AARect worldBounds;
			float  hexRadius = 1.0f;
		};
	}
}
