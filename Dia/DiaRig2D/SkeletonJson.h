#pragma once

#include "ISkeletonLoader.h"

namespace Dia
{
	namespace Rig2D
	{
		class JsonSkeletonLoader : public ISkeletonLoader
		{
		public:
			bool Load(const char* data, SkeletonDef& outDef) const override;
			bool Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const override;
		};
	}
}
