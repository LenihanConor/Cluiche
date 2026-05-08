#pragma once

#include "ISkeletonSerializer.h"

namespace Dia
{
	namespace Rig2D
	{
		class JsonSkeletonSerializer : public ISkeletonSerializer
		{
		public:
			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Load(const char* data, SkeletonDef& outDef) const override;
			Dia::Serializer::SerializeResult Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const override;
		};
	}
}
