#pragma once

#include <DiaSerializer/ISerializer.h>
#include <DiaSerializer/SerializeResult.h>
#include <DiaGeometry2D/Spatial/SpatialGridDef.h>

namespace Dia
{
	namespace Geometry2D
	{
		class ISpatialGridSerializer : public Dia::Serializer::ISerializer
		{
		public:
			virtual Dia::Serializer::SerializeResult LoadSpatialGrid(const char* data, SpatialGridConfig& outCfg) const = 0;
			virtual Dia::Serializer::SerializeResult SaveSpatialGrid(const SpatialGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const = 0;

			virtual Dia::Serializer::SerializeResult LoadHexGrid(const char* data, HexGridConfig& outCfg) const = 0;
			virtual Dia::Serializer::SerializeResult SaveHexGrid(const HexGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const = 0;

			Dia::Serializer::SerializeResult LoadSpatialGridFromFile(const char* path, SpatialGridConfig& outCfg) const;
			Dia::Serializer::SerializeResult SaveSpatialGridToFile(const char* path, const SpatialGridConfig& cfg) const;

			Dia::Serializer::SerializeResult LoadHexGridFromFile(const char* path, HexGridConfig& outCfg) const;
			Dia::Serializer::SerializeResult SaveHexGridToFile(const char* path, const HexGridConfig& cfg) const;
		};
	}
}
