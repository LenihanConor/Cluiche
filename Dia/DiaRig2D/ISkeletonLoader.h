#pragma once

#include "Skeleton.h"

namespace Dia
{
	namespace Rig2D
	{
		class ISkeletonLoader
		{
		public:
			virtual ~ISkeletonLoader() {}

			virtual bool Load(const char* data, SkeletonDef& outDef) const = 0;
			virtual bool Save(const SkeletonDef& def, char* outBuffer, unsigned int bufferSize) const = 0;
		};
	}
}
