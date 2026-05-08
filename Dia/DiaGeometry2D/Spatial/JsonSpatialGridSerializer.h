#pragma once

#include <DiaGeometry2D/Spatial/ISpatialGridSerializer.h>

namespace Dia
{
	namespace Geometry2D
	{
		class JsonSpatialGridSerializer : public ISpatialGridSerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult LoadSpatialGrid(const char* data, SpatialGridConfig& outCfg) const override;
			Dia::Serializer::SerializeResult SaveSpatialGrid(const SpatialGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const override;

			Dia::Serializer::SerializeResult LoadHexGrid(const char* data, HexGridConfig& outCfg) const override;
			Dia::Serializer::SerializeResult SaveHexGrid(const HexGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const override;
		};
	}
}
