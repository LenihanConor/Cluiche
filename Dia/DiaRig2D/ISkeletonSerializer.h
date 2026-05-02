#pragma once

#include "Skeleton.h"
#include <DiaSerializer/SerializeResult.h>
#include <DiaSerializer/ISerializer.h>

namespace Dia
{
	namespace Rig2D
	{
		class ISkeletonSerializer : public Dia::Serializer::ISerializer
		{
		public:
			virtual ~ISkeletonSerializer() {}

			virtual Dia::Serializer::SerializeResult Load(const char* data, SkeletonDef& outDef) const = 0;
			virtual Dia::Serializer::SerializeResult Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const = 0;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, SkeletonDef& outDef) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const SkeletonDef& def) const;
		};
	}
}
